#pragma once
#include "geometry_culling.h"
#include <limits>
inline void GeometryCullingTests() {
    using namespace mmvr;
    struct Command {
        struct {
            uintptr_t w0 = 0, w1 = 0;
        } words;
    } commands[66]{};
    for (int i = 0; i < 64; ++i)
        commands[i].words = { 0x06000204, 0x00040608 };
    auto packet = CompileTrianglePacket(commands);
    check(packet.commands == 64 && packet.count == 5);
    commands[3].words.w0 = 0xDA000000; // Matrix/state boundary may never be skipped.
    packet = CompileTrianglePacket(commands);
    check(packet.commands == 3 && packet.count == 5);
    commands[1].words.w0 = 0x06010204;
    check(CompileTrianglePacket(commands).commands == 0);
    commands[1].words.w0 = 0x06000288;
    check(CompileTrianglePacket(commands).commands == 0);
    commands[1].words.w0 = 0x05000204;
    packet = CompileTrianglePacket(commands);
    check(packet.commands == 3);
    commands[2].words.w0 = 0xDE000000;
    check(CompileTrianglePacket(commands).commands == 0);
    const auto guard = MakeCullingGuard({ -.8f, .8f, .8f, -.8f }, 20);
    check(guard.active && guard.horizontal > 2 && guard.vertical > 2);
    check(!MakeCullingGuard({ -1.5f, 1.5f, .8f, -.8f }, 20).active);
    check(!MakeCullingGuard({ 0, 0, 0, 0 }, 20).active);
    struct Vertex {
        float x = 0, y = 0, z = 0, w = 1;
    } v[68];
    for (auto& vertex : v)
        vertex.x = guard.horizontal + 1;
    check(PacketOutsideGuard(packet, v, 68, guard));
    v[4].x = 0;
    check(!PacketOutsideGuard(packet, v, 68, guard)); // A large object spans the edge.
    for (auto& vertex : v)
        vertex.x = guard.horizontal;
    check(!PacketOutsideGuard(packet, v, 68, guard));
    for (auto& vertex : v)
        vertex.x = 1.5f;
    check(!PacketOutsideGuard(packet, v, 68, guard)); // Safety band.
    v[0].x = std::numeric_limits<float>::quiet_NaN();
    check(!PacketOutsideGuard(packet, v, 68, guard));
    for (auto& vertex : v) {
        vertex.x = 0;
        vertex.w = -1;
    }
    check(PacketOutsideGuard(packet, v, 68, guard));
    v[0].w = 1;
    check(!PacketOutsideGuard(packet, v, 68, guard)); // Crosses the viewer.
    check(!PacketOutsideGuard(packet, v, 2, guard));
    check(!PacketOutsideGuard(packet, v, 68, {}));
    const auto asymmetric = MakeCullingGuard({ -.55f, .9f, .7f, -.85f }, 20);
    check(asymmetric.active);
    TurnCullingGuard turn;
    check(turn.Update({ 0, 0, 0, 1 }, 1) == 0);
    check(turn.Update({ 0, .1f, 0, std::sqrt(.99f) }, 1.01) > 10);
    check(turn.Update({ 0, .1f, 0, std::sqrt(.99f) }, 1.02) == 0);
    check(turn.Update({ 0, 0, 0, 0 }, 1.03) == 0);
    check(turn.Update({ 0, 0, 0, 1 }, 2) == 0);
    // Cached topology never caches a visibility decision: moving into view must
    // immediately restore a previously rejected packet, with no frame hysteresis.
    for (int angle = 0; angle < 360; ++angle) {
        const float r = angle * .017453292519943295f;
        for (int i = 0; i < 5; ++i) {
            v[i].x = 10 * std::sin(r) + i * .01f;
            v[i].y = 0;
            v[i].w = 10 * std::cos(r);
        }
        if (std::abs(angle) <= 40 || angle >= 320)
            check(!PacketOutsideGuard(packet, v, 68, guard));
    }
    struct TestResolver {
        std::array<float,3> points[4]{{10,0,0},{11,0,0},{10,1,0},{11,1,0}};
        bool operator()(const Command* c, GeometryLoad& load) const {
            if ((c->words.w0>>24)!=1) return false;
            load={points,uint16_t(c->words.w1),uint16_t(c->words.w0&255),1};
            return true;
        }
        std::array<float,3> Position(const GeometryLoad& load,unsigned index) const {
            return static_cast<const std::array<float,3>*>(load.vertices)[index];
        }
    } resolver;
    Command strip[14]{};
    for(unsigned group=0;group<3;++group) {
        strip[group*4].words={0x01000000,4};
        for(unsigned tri=1;tri<=3;++tri) strip[group*4+tri].words={0x06000204,0x00020406};
    }
    strip[12].words={0xDA000000,0};
    auto run=CompileGeometryRun(strip,resolver);
    check(run.commands==12 && run.vertices==12 && run.retainedVertices==4 && run.retained.size()==1);
    float matrix[4][4]{};for(unsigned a=0;a<4;++a)matrix[a][a]=1;
    check(GeometryOutsideGuard(run,matrix,guard));
    matrix[3][0]=-10;check(!GeometryOutsideGuard(run,matrix,guard));
    // Paired geometry can be discarded only if neither padded eye sees it.
    float other[4][4]{};for(unsigned a=0;a<4;++a)other[a][a]=1;
    check(!GeometryOutsideBothGuards(run,matrix,guard,other,guard));
    matrix[3][0]=0;check(GeometryOutsideBothGuards(run,matrix,guard,other,guard));
    check(!GeometryOutsideBothGuards(run,matrix,guard,other,{}));
    const auto tight=MakeCullingGuard({-.8f,.8f,.8f,-.8f},0);
    matrix[3][0]=-8;other[3][0]=-8;
    check(GeometryOutsideBothGuards(run,matrix,tight,other,tight));
    check(!GeometryOutsideBothGuards(run,matrix,guard,other,guard));
    other[3][0]=0;matrix[3][0]=0;
    matrix[3][0]=0;matrix[0][0]=std::numeric_limits<float>::quiet_NaN();
    check(!GeometryOutsideGuard(run,matrix,guard));
    // Partial final overwrite must retain the preceding write as well.
    strip[8].words.w1=3;
    for(unsigned t=9;t<12;++t)strip[t].words.w1=0x00020400;
    run=CompileGeometryRun(strip,resolver);
    check(run.retained.size()==2 && run.retainedVertices==7 && run.vertices==11);
    strip[1].words.w0=0x06000210; // Read a slot never loaded in this run.
    check(!CompileGeometryRun(strip,resolver).commands);
    strip[1].words.w0=0x06000204;strip[4].words.w0=0xDA000000;
    check(!CompileGeometryRun(strip,resolver).commands);

}
