#pragma once
#include "gles_bridge.h"
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <fstream>
#include <cstring>
#include <cstdlib>
namespace mmvr {
// GLES cannot glReadPixels(GL_DEPTH_STENCIL). Copy only the requested depth
// texels to a sampleable attachment, encode native depth into RG, then read RGBA.
class GlDepthReadback {
    GLuint depth = 0, depthFbo = 0, program = 0, vao = 0;
    GLint depthLocation = -1, directLocation = -1, pointLocation = -1;
    size_t capacity = 0;
    EGLContext context = EGL_NO_CONTEXT;
    GlTarget color, stagedColor;
    GLuint stagedBuffer=0, coordinateTexture=0, stagedSource=0;
    GLsync stagedFence=nullptr;
    unsigned stagedWidth=0,stagedHeight=0;
    uint64_t gridGeneration=0;
    size_t stagedBytes=0;
    bool stagedInverted=false, mapped=false;
    std::unordered_map<uint64_t,size_t> sampleIndices;
    std::vector<unsigned char> stagedValues;
    unsigned long long stages=0,hits=0,misses=0,verified=0;
    static uint64_t SampleKey(int x,int y){return (uint64_t(uint32_t(x))<<32)|uint32_t(y);}
    static bool Reference(){
        static const bool value=[](){const char* p=std::getenv("MMVR_ASYNC_DEPTH_REFERENCE");return p && !std::strcmp(p,"1");}();
        return value;
    }
    void Report(){
        if(!std::getenv("MMVR_NATIVE_TEST"))return;
        std::ofstream("native-async-depth.json")<<"{\"stages\":"<<stages<<",\"hits\":"<<hits
            <<",\"fallbacks\":"<<misses<<",\"verified\":"<<verified<<",\"bytes\":"<<stagedBytes<<"}";
    }
    struct LookupState {
        GLint active=0, texture=0, sampler=0,unpack=0;
        GLint values[4]{};
        const GLenum names[4]{GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_PIXELS};
        LookupState(){
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE1);
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);glBindSampler(1,0);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
            for(int i=0;i<4;++i){glGetIntegerv(names[i],&values[i]);glPixelStorei(names[i],i?0:1);}
        }
        ~LookupState(){
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,texture);glBindSampler(1,sampler);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);
            for(int i=0;i<4;++i)glPixelStorei(names[i],values[i]);glActiveTexture(active);
        }
    };
    bool ReadStaged(GLuint source,unsigned width,unsigned height,bool inverted,
                    const std::vector<std::array<int,2>>& points,std::vector<uint16_t>& output){
        if(eglGetCurrentContext()!=context || Reference() || source!=stagedSource || width!=stagedWidth || height!=stagedHeight || inverted!=stagedInverted || !stagedFence)
            return false;
        for(const auto& p:points){
            const int x=std::clamp(p[0],0,int(width)-1),y=std::clamp(inverted?int(height)-p[1]:p[1],0,int(height)-1);
            if(!sampleIndices.count(SampleKey(x,y)))return false;
        }
        if(!mapped){
            const GLenum ready=glClientWaitSync(stagedFence,0,0);
            if(ready!=GL_ALREADY_SIGNALED && ready!=GL_CONDITION_SATISFIED)return false;
            GLint old=0;glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&old);glBindBuffer(GL_PIXEL_PACK_BUFFER,stagedBuffer);
            const auto* bytes=static_cast<const unsigned char*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,stagedBytes,GL_MAP_READ_BIT));
            if(!bytes){glBindBuffer(GL_PIXEL_PACK_BUFFER,old);return false;}
            stagedValues.assign(bytes,bytes+stagedBytes);
            const bool valid=glUnmapBuffer(GL_PIXEL_PACK_BUFFER)==GL_TRUE;
            glBindBuffer(GL_PIXEL_PACK_BUFFER,old);
            if(!valid){Invalidate(source);return false;}mapped=true;
        }
        output.resize(points.size());
        for(size_t i=0;i<points.size();++i){
            const auto& p=points[i];const int x=std::clamp(p[0],0,int(width)-1),y=std::clamp(inverted?int(height)-p[1]:p[1],0,int(height)-1);
            const size_t offset=sampleIndices.at(SampleKey(x,y))*4;
            output[i]=uint16_t(stagedValues[offset])|(uint16_t(stagedValues[offset+1])<<8);
        }
        return true;
    }

    struct State {
        GlFramebufferState framebuffers;
        GLint program, vao, active, texture, sampler, pack, viewport[4];
        const std::array<GLenum, 4> packNames{ GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS,
                                               GL_PACK_SKIP_PIXELS };
        std::array<GLint, 4> packValues{};
        GLboolean mask[4];
        const std::array<GLenum, 8> caps{ GL_BLEND,
                                          GL_DEPTH_TEST,
                                          GL_STENCIL_TEST,
                                          GL_CULL_FACE,
                                          GL_DITHER,
                                          GL_SAMPLE_ALPHA_TO_COVERAGE,
                                          GL_RASTERIZER_DISCARD,
                                          GL_SAMPLE_COVERAGE };
        std::array<GLboolean, 8> enabled{};
        State() {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetBooleanv(GL_COLOR_WRITEMASK, mask);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            for (size_t i = 0; i < packNames.size(); ++i) {
                glGetIntegerv(packNames[i], &packValues[i]);
                glPixelStorei(packNames[i], i == 0 ? 1 : 0);
            }
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
            glBindSampler(0, 0);
            for (size_t i = 0; i < caps.size(); ++i) {
                enabled[i] = glIsEnabled(caps[i]);
                glDisable(caps[i]);
            }
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        ~State() {
            glUseProgram(program);
            glBindVertexArray(vao);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glColorMask(mask[0], mask[1], mask[2], mask[3]);
            for (size_t i = 0; i < caps.size(); ++i)
                if (enabled[i])
                    glEnable(caps[i]);
            glBindTexture(GL_TEXTURE_2D, texture);
            glBindSampler(0, sampler);
            glActiveTexture(active);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
            for (size_t i = 0; i < packNames.size(); ++i)
                glPixelStorei(packNames[i], packValues[i]);
        }
    };
    void Init() {
        const auto current = eglGetCurrentContext();
        if (current == EGL_NO_CONTEXT)
            throw std::runtime_error("Depth readback requires a current context");
        if (context != current) {
            // Never reuse or delete names/fences belonging to a replaced context.
            depth = depthFbo = program = vao = 0;
            stagedBuffer = coordinateTexture = stagedSource = 0;
            stagedFence = nullptr;
            capacity = stagedBytes = 0;
            stagedWidth = stagedHeight = 0;
            gridGeneration = 0;
            mapped = false;
            sampleIndices.clear();
            stagedValues.clear();
            color.Reset();
            stagedColor.Reset();
            context = current;
        }
        if (program)
            return;
        auto shader = [](GLenum type, const char* source) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &source, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                glDeleteShader(s);
                throw std::runtime_error("Depth readback shader compile failed");
            }
            return s;
        };
        const char* vs =
            "#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}";
        const char* fs =
            "#version 300 es\nprecision highp float;precision highp int;uniform highp sampler2D depthTex;uniform int directMode;uniform ivec2 sampleCoords[64];uniform highp isampler2D coordinateTex;out vec4 pixel;void main(){ivec2 p=directMode==2?texelFetch(coordinateTex,ivec2(gl_FragCoord.xy),0).rg:(directMode!=0?sampleCoords[int(gl_FragCoord.x)]:ivec2(int(gl_FragCoord.x),0));float d=texelFetch(depthTex,p,0).r;uint n=(uint(round(clamp(d,0.0,1.0)*16777215.0))>>10u)<<2u;pixel=vec4(float(n&255u),float((n>>8u)&255u),0,255)/255.0;}";
        // Publish the program only after linking succeeds. A failed attempt must
        // leave Init retryable and release both shaders, including when the second
        // shader fails before a program has been created.
        GLuint v = 0, f = 0, linked = 0;
        try {
            v = shader(GL_VERTEX_SHADER, vs);
            f = shader(GL_FRAGMENT_SHADER, fs);
            linked = glCreateProgram();
            glAttachShader(linked, v);
            glAttachShader(linked, f);
            glLinkProgram(linked);
            GLint ok = 0;
            glGetProgramiv(linked, GL_LINK_STATUS, &ok);
            if (!ok)
                throw std::runtime_error("Depth readback shader link failed");
            glGenVertexArrays(1, &vao);
            program = linked;
            depthLocation = glGetUniformLocation(program, "depthTex");
            directLocation = glGetUniformLocation(program, "directMode");
            pointLocation = glGetUniformLocation(program, "sampleCoords[0]");
        } catch (...) {
            if (linked) glDeleteProgram(linked);
            if (f) glDeleteShader(f);
            if (v) glDeleteShader(v);
            throw;
        }
        glDeleteShader(v);
        glDeleteShader(f);
    }
    void Resize(size_t count) {
        // Only the requested prefix is drawn/read. Retain the high-water capacity
        // when visibility changes the sample count rather than reallocating both
        // attachments every frame (the desktop backend already does this).
        if (capacity >= count)
            return;
        capacity = 0; // An allocation failure must not leave stale capacity valid.
        if (depthFbo)
            glDeleteFramebuffers(1, &depthFbo);
        if (depth)
            glDeleteTextures(1, &depth);
        glGenTextures(1, &depth);
        glBindTexture(GL_TEXTURE_2D, depth);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, GLsizei(count), 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glGenFramebuffers(1, &depthFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, depthFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Depth readback attachment incomplete: status=" + std::to_string(status) +
                                     " count=" + std::to_string(count) + " error=" + std::to_string(glGetError()));
        color.Resize(unsigned(count), 1);
        capacity = count;
    }

  public:
    ~GlDepthReadback() {
        if (eglGetCurrentContext() == context) {
            if(stagedFence)glDeleteSync(stagedFence);
            if(stagedBuffer)glDeleteBuffers(1,&stagedBuffer);
            if(coordinateTexture)glDeleteTextures(1,&coordinateTexture);
            if (depthFbo)
                glDeleteFramebuffers(1, &depthFbo);
            if (depth)
                glDeleteTextures(1, &depth);
            if (program)
                glDeleteProgram(program);
            if (vao)
                glDeleteVertexArrays(1, &vao);
        } else {
            color.image = {};
            stagedColor.image = {};
        }
    }
    void Invalidate(GLuint source){
        if(source!=stagedSource)return;
        stagedSource=0;mapped=false;
        if(stagedFence){if(context==eglGetCurrentContext())glDeleteSync(stagedFence);stagedFence=nullptr;}
    }
    void Stage(GLuint source,unsigned width,unsigned height,bool inverted,
               const std::vector<std::pair<float,float>>& coordinates,uint64_t generation,GLuint texture){
        if(Reference() || !texture || coordinates.empty() || !width || !height || eglGetCurrentContext()==EGL_NO_CONTEXT)return;
        State saved;Init();LookupState lookup;
        constexpr unsigned columns=256;
        const unsigned rows=unsigned((coordinates.size()+columns-1)/columns);
        const size_t bytes=size_t(columns)*rows*4;
        if(gridGeneration!=generation || width!=stagedWidth || height!=stagedHeight || inverted!=stagedInverted || !coordinateTexture){
            std::vector<GLint> pixels(size_t(columns)*rows*2,0);
            sampleIndices.clear();sampleIndices.reserve(coordinates.size());
            for(size_t i=0;i<coordinates.size();++i){
                const int x=std::clamp(int(coordinates[i].first),0,int(width)-1);
                const int y=std::clamp(inverted?int(height)-int(coordinates[i].second):int(coordinates[i].second),0,int(height)-1);
                pixels[i*2]=x;pixels[i*2+1]=y;sampleIndices.emplace(SampleKey(x,y),i);
            }
            if(!coordinateTexture)glGenTextures(1,&coordinateTexture);
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,coordinateTexture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RG32I,columns,rows,0,GL_RG_INTEGER,GL_INT,pixels.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            gridGeneration=generation;
        }
        if(stagedFence){if(context==eglGetCurrentContext())glDeleteSync(stagedFence);stagedFence=nullptr;}
        stagedColor.Resize(columns,rows);
        glBindFramebuffer(GL_FRAMEBUFFER,stagedColor.image.framebuffer);glViewport(0,0,columns,rows);
        glUseProgram(program);glUniform1i(depthLocation,0);glUniform1i(directLocation,2);
        glUniform1i(glGetUniformLocation(program,"coordinateTex"),1);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,coordinateTexture);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES,0,3);
        if(!stagedBuffer)glGenBuffers(1,&stagedBuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,stagedBuffer);
        if(stagedBytes!=bytes)glBufferData(GL_PIXEL_PACK_BUFFER,bytes,nullptr,GL_STREAM_READ);
        glReadPixels(0,0,columns,rows,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        stagedFence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
        stagedSource=source;stagedWidth=width;stagedHeight=height;stagedInverted=inverted;stagedBytes=bytes;mapped=false;++stages;
    }
    std::vector<uint16_t> Read(GLuint source, unsigned width, unsigned height, bool inverted,
                               const std::vector<std::array<int, 2>>& points, GLuint sourceDepthTexture = 0, bool allowStaged = true) {
        if (points.empty())
            return {};
        if (!width || !height)
            return std::vector<uint16_t>(points.size(), 0xfffc);
        // SDL can release the current context while Android pauses its surface. No
        // visible frame exists then; defer sampling until the same context resumes.
        if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
            static bool logged = false;
            if (!logged) {
                fprintf(stderr, "MMVR depth readback deferred: no current EGL context\n");
                logged = true;
            }
            return std::vector<uint16_t>(points.size(), 0xfffc);
        }
        if(allowStaged){
            std::vector<uint16_t> staged;
            if(ReadStaged(source,width,height,inverted,points,staged)){
                ++hits;
                if(std::getenv("MMVR_ASYNC_DEPTH_VERIFY")){
                    auto reference=Read(source,width,height,inverted,points,sourceDepthTexture,false);
                    if(reference!=staged)throw std::runtime_error("Asynchronous native depth differs from synchronous reference");
                    ++verified;
                }
                if((hits+misses)%60==0)Report();
                return staged;
            }
            ++misses;if((hits+misses)%60==0)Report();
        }
        State saved;
        Init();
        Resize(points.size());
        // The owned single-sample native FBO has the same DEPTH24_STENCIL8
        // attachment as before, now sampleable. One shader read per requested
        // pixel avoids N driver blits and retains the exact depth conversion.
        const bool direct = sourceDepthTexture && points.size() <= 64;
        std::array<GLint, 128> coordinates{};
        if (direct) {
            for (size_t i = 0; i < points.size(); ++i) {
                coordinates[i * 2] = std::clamp(points[i][0], 0, int(width) - 1);
                coordinates[i * 2 + 1] = std::clamp(inverted ? int(height) - points[i][1] : points[i][1],
                                                    0, int(height) - 1);
            }
        } else {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthFbo);
            for (size_t i = 0; i < points.size(); ++i) {
                int x = std::clamp(points[i][0], 0, int(width) - 1),
                    y = std::clamp(inverted ? int(height) - points[i][1] : points[i][1], 0, int(height) - 1);
                glBlitFramebuffer(x, y, x + 1, y + 1, GLint(i), 0, GLint(i + 1), 1,
                                  GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, color.image.framebuffer);
        glViewport(0, 0, GLsizei(points.size()), 1);
        glUseProgram(program);
        glUniform1i(depthLocation, 0);
        glUniform1i(glGetUniformLocation(program,"coordinateTex"),1);
        glUniform1i(directLocation, direct ? 1 : 0);
        if (direct) glUniform2iv(pointLocation, GLsizei(points.size()), coordinates.data());
        glBindVertexArray(vao);
        glBindTexture(GL_TEXTURE_2D, direct ? sourceDepthTexture : depth);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<unsigned char> bytes(points.size() * 4);
        glReadPixels(0, 0, GLsizei(points.size()), 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        std::vector<uint16_t> values(points.size());
        for (size_t i = 0; i < points.size(); ++i)
            values[i] = uint16_t(bytes[i * 4]) | (uint16_t(bytes[i * 4 + 1]) << 8);
        return values;
    }
};
} // namespace mmvr
