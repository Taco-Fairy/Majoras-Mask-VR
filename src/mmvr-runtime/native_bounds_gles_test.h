#pragma once
// Protected Quest-only diagnostic. Normal play never reads back framebuffers.
#include "gles_bridge.h"
#include "culling_audit.h"
#include "packet_profile.h"
#include <vector>
#include <fstream>
#include <algorithm>
namespace mmvr {
inline std::vector<unsigned char> NativeBoundsReadPixels(const GlImage& image) {
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
        size_t(image.width) * image.height > 16 * 1024 * 1024)
        throw std::runtime_error("Invalid bounds comparison target");
    GlFramebufferState framebuffer;
    struct PackRestore {
        GLint buffer = 0;
        const GLenum names[4] = { GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS };
        GLint values[4]{};
        PackRestore() {
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
            for (int i = 0; i < 4; ++i)
                glGetIntegerv(names[i], &values[i]);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            for (int i = 0; i < 4; ++i)
                glPixelStorei(names[i], i == 0 ? 1 : 0);
        }
        ~PackRestore() {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
            for (int i = 0; i < 4; ++i)
                glPixelStorei(names[i], values[i]);
        }
    } pack;
    GlTarget resolved;
    resolved.Resize(image.width, image.height, image.inverted);
    GlBlit(image, resolved.image);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolved.image.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    std::vector<unsigned char> bytes(size_t(image.width) * image.height * 4);
    glReadPixels(0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Bounds comparison readback failed");
    return bytes;
}
inline void NativeBoundsPixelComparison(const GlImage& source, const std::function<void(bool)>& draw,
                                        const std::string& label, int eye) {
    struct Restore {
        bool saved = cullingReplayReference;
        ~Restore() {
            cullingReplayReference = saved;
        }
    } restore;
    cullingReplayReference = true;
    draw(false);
    auto reference = NativeBoundsReadPixels(source);
    cullingReplayReference = false;
    const auto packetsBefore = cullingAudit.skippedPackets;
    const auto verticesBefore = cullingAudit.skippedPacketVertices;
    const auto commandsBefore = cullingAudit.skippedPacketCommands;
    auto skippedBefore = cullingAudit.skippedLists;
    draw(false);
    auto candidate = NativeBoundsReadPixels(source);
    const auto skipped = cullingAudit.skippedLists - skippedBefore;
    if (reference.size() != candidate.size())
        throw std::runtime_error("Bounds comparison size changed");
    size_t nonUniform = 0;
    for (size_t i = 4; i < reference.size(); i += 4)
        nonUniform +=
            reference[i] != reference[0] || reference[i + 1] != reference[1] || reference[i + 2] != reference[2];
    size_t rgb = 0, alpha = 0;
    unsigned maxDelta = 0;
    for (size_t i = 0; i < reference.size(); ++i) {
        unsigned delta = unsigned(std::abs(int(reference[i]) - int(candidate[i])));
        maxDelta = std::max(maxDelta, delta);
        if (delta) {
            if (i % 4 == 3)
                ++alpha;
            else
                ++rgb;
        }
    }
    static std::ofstream log = []() {
        std::ofstream out("native-bounds-pixels.log");
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        out << "session " << (token ? token : "") << "\n";
        return out;
    }();
    log << label << " eye=" << eye << " width=" << source.width << " height=" << source.height
        << " bytes=" << reference.size() << " rgbDifferences=" << rgb << " alphaDifferences=" << alpha
        << " nonUniformPixels=" << nonUniform << " maxDelta=" << maxDelta << " skippedLists=" << skipped
        << " skippedPackets=" << (cullingAudit.skippedPackets - packetsBefore)
        << " skippedPacketVertices=" << (cullingAudit.skippedPacketVertices - verticesBefore) << " skippedPacketCommands=" << (cullingAudit.skippedPacketCommands - commandsBefore) << "\n"
        << std::flush;
    static unsigned failures = 0;
    if ((rgb || alpha) && failures++ < 2) {
        auto write = [&](const std::vector<unsigned char>& bytes, const char* suffix) {
            std::ofstream out(std::string("native-bounds-mismatch-") + std::to_string(failures) + suffix + ".ppm",
                              std::ios::binary);
            out << "P6\n" << source.width << " " << source.height << "\n255\n";
            for (unsigned y = 0; y < source.height; ++y) {
                unsigned row = source.inverted ? y : source.height - 1 - y;
                for (unsigned x = 0; x < source.width; ++x)
                    out.write(reinterpret_cast<const char*>(bytes.data() + 4 * (size_t(row) * source.width + x)), 3);
            }
        };
        write(reference, "-reference");
        write(candidate, "-candidate");
    }
    ProfilePacketCulling(label, eye, [] {}, [&] { draw(false); }, [&] { return NativeBoundsReadPixels(source); });
}
} // namespace mmvr
