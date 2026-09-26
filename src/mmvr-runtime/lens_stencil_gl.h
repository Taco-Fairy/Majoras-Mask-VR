#pragma once
#include "lens_aperture.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#ifdef __ANDROID__
#include <EGL/egl.h>
#endif
namespace mmvr {
// Only used with a current GL context. Saves every state modified by the stencil
// geometry, including independent front/back stencil state and renderer VAOs.
struct LensGlState {
    GLint program = 0, vao = 0, buffer = 0, viewport[4]{}, scissor[4]{}, clearStencil = 0;
    GLboolean color[4]{}, depthMask = false;
    struct Face {
        GLint func, ref, valueMask, writeMask, fail, zfail, zpass;
    } front{}, back{};
    static constexpr GLenum caps[] = { GL_BLEND,
                                       GL_DEPTH_TEST,
                                       GL_SCISSOR_TEST,
                                       GL_CULL_FACE,
                                       GL_POLYGON_OFFSET_FILL,
                                       GL_SAMPLE_ALPHA_TO_COVERAGE,
                                       GL_RASTERIZER_DISCARD,
                                       GL_STENCIL_TEST,
                                       GL_SAMPLE_COVERAGE };
    bool enabled[9]{};
    LensGlState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetBooleanv(GL_COLOR_WRITEMASK, color);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        front = Read(false);
        back = Read(true);
        for (int i = 0; i < 9; ++i)
            enabled[i] = glIsEnabled(caps[i]);
    }
    static Face Read(bool b) {
        Face f{};
        glGetIntegerv(b ? GL_STENCIL_BACK_FUNC : GL_STENCIL_FUNC, &f.func);
        glGetIntegerv(b ? GL_STENCIL_BACK_REF : GL_STENCIL_REF, &f.ref);
        glGetIntegerv(b ? GL_STENCIL_BACK_VALUE_MASK : GL_STENCIL_VALUE_MASK, &f.valueMask);
        glGetIntegerv(b ? GL_STENCIL_BACK_WRITEMASK : GL_STENCIL_WRITEMASK, &f.writeMask);
        glGetIntegerv(b ? GL_STENCIL_BACK_FAIL : GL_STENCIL_FAIL, &f.fail);
        glGetIntegerv(b ? GL_STENCIL_BACK_PASS_DEPTH_FAIL : GL_STENCIL_PASS_DEPTH_FAIL, &f.zfail);
        glGetIntegerv(b ? GL_STENCIL_BACK_PASS_DEPTH_PASS : GL_STENCIL_PASS_DEPTH_PASS, &f.zpass);
        return f;
    }
    static void RestoreFace(GLenum side, const Face& f) {
        glStencilFuncSeparate(side, f.func, f.ref, f.valueMask);
        glStencilMaskSeparate(side, f.writeMask);
        glStencilOpSeparate(side, f.fail, f.zfail, f.zpass);
    }
    ~LensGlState() {
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        glColorMask(color[0], color[1], color[2], color[3]);
        glDepthMask(depthMask);
        glClearStencil(clearStencil);
        RestoreFace(GL_FRONT, front);
        RestoreFace(GL_BACK, back);
        for (int i = 0; i < 9; ++i) {
            if (enabled[i])
                glEnable(caps[i]);
            else
                glDisable(caps[i]);
        }
    }
};
class LensStencilGl {
    // Android's owned context is EGL, including the offscreen GPU verifier.
    static void* CurrentContext() {
#ifdef __ANDROID__
        return eglGetCurrentContext();
#else
        return SDL_GL_GetCurrentContext();
#endif
    }
    void* creator = nullptr;
    GLuint program = 0, vao = 0;
    GLint aperture = -1, points = -1, shape = -1, z = -1, color = -1;

  public:
    ~LensStencilGl() {
        if (creator && creator == CurrentContext()) {
            if (program)
                glDeleteProgram(program);
            if (vao)
                glDeleteVertexArrays(1, &vao);
        }
    }
    void Init() {
        auto context = CurrentContext();
        if (!context)
            throw std::runtime_error("Lens stencil requires a current GL context");
        if (creator != context) {
            program = vao = 0;
            creator = context;
        }
        if (program)
            return;
#ifdef __ANDROID__
        const char* version = "#version 300 es\nprecision highp float;\n";
#else
        const char* version = "#version 330 core\n";
#endif
        const std::string vs =
            std::string(version) + "#define MMVR_LENS_SEGMENTS " + std::to_string(LensSegments) + "\n" + R"(
uniform vec4 aperture;uniform vec2 points[MMVR_LENS_SEGMENTS];uniform int shape;uniform float depth;
void main(){vec2 p;if(shape==0){p=gl_VertexID==0?vec2(-1,-1):gl_VertexID==1?vec2(3,-1):vec2(-1,3);}else{p=gl_VertexID==0?aperture.xy:points[(gl_VertexID-1)%MMVR_LENS_SEGMENTS];p=p*2.0-1.0;}gl_Position=vec4(p,depth,1);})";
        const std::string fs = std::string(version) + "uniform vec4 color;out vec4 result;void main(){result=color;}";
        GLuint shaders[2]{};
        GLuint candidate = glCreateProgram();
        for (int i = 0; i < 2; ++i) {
            shaders[i] = glCreateShader(i ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
            const char* source = i ? fs.c_str() : vs.c_str();
            glShaderSource(shaders[i], 1, &source, nullptr);
            glCompileShader(shaders[i]);
            GLint ok = 0;
            glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &ok);
            if (!ok) {
                for (GLuint s : shaders)
                    if (s)
                        glDeleteShader(s);
                glDeleteProgram(candidate);
                throw std::runtime_error("Lens stencil shader compilation failed");
            }
            glAttachShader(candidate, shaders[i]);
        }
        glLinkProgram(candidate);
        GLint ok = 0;
        glGetProgramiv(candidate, GL_LINK_STATUS, &ok);
        for (GLuint s : shaders) {
            glDetachShader(candidate, s);
            glDeleteShader(s);
        }
        if (!ok) {
            glDeleteProgram(candidate);
            throw std::runtime_error("Lens stencil shader link failed");
        }
        program = candidate;
        glGenVertexArrays(1, &vao);
        aperture = glGetUniformLocation(program, "aperture");
        points = glGetUniformLocation(program, "points");
        shape = glGetUniformLocation(program, "shape");
        z = glGetUniformLocation(program, "depth");
        color = glGetUniformLocation(program, "color");
    }
    void Draw(bool circle, float depth, const std::array<float, 4>& rgba) {
        Init();
        auto a = GetLensAperture();
        glUseProgram(program);
        glBindVertexArray(vao);
        auto center = LensCenter();
        std::array<std::array<float, 2>, LensSegments> edge{};
        for (int i = 0; i < LensSegments; ++i)
            edge[i] = LensBoundary(i);
        glUniform4f(aperture, center[0], center[1], a.rx, a.ry);
        glUniform2fv(points, LensSegments, edge[0].data());
        glUniform1i(shape, circle ? 1 : 0);
        glUniform1f(z, depth * 2 - 1);
        glUniform4fv(color, 1, rgba.data());
        glDrawArrays(circle ? GL_TRIANGLE_FAN : GL_TRIANGLES, 0, circle ? LensSegments + 2 : 3);
    }
    void Prepare(int width, int height) {
        LensGlState saved;
        for (GLenum cap : LensGlState::caps)
            glDisable(cap);
        glViewport(0, 0, width, height);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glEnable(GL_STENCIL_TEST);
        glStencilMask(1);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        Draw(true, 0, { 0, 0, 0, 0 });
    }
    static void Apply(int mode) {
        if (!mode) {
            glDisable(GL_STENCIL_TEST);
            return;
        }
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(mode == 1 ? GL_EQUAL : GL_NOTEQUAL, 1, 1);
    }
    void Verify() {
        LensGlState saved;
        GLint oldRead = 0, oldDraw = 0, oldTexture = 0, oldRb = 0, oldPack = 0, oldAlignment = 0, oldRow = 0,
              oldSkipRows = 0, oldSkipPixels = 0, oldDepthFunc = 0, oldUnpack = 0;
        GLfloat clear[4]{}, clearDepth = 1;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDraw);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRb);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &oldPack);
        glGetIntegerv(GL_PACK_ALIGNMENT, &oldAlignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &oldRow);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &oldSkipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &oldSkipPixels);
        glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldUnpack);
        struct Cleanup {
            GLint rd, dr, tex, rb, pack, alignment, row, rows, pixels, depth, unpack;
            GLfloat* clear;
            GLfloat clearDepth;
            ~Cleanup() {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, rd);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dr);
                glBindTexture(GL_TEXTURE_2D, tex);
                glBindRenderbuffer(GL_RENDERBUFFER, rb);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
                glPixelStorei(GL_PACK_ALIGNMENT, alignment);
                glPixelStorei(GL_PACK_ROW_LENGTH, row);
                glPixelStorei(GL_PACK_SKIP_ROWS, rows);
                glPixelStorei(GL_PACK_SKIP_PIXELS, pixels);
                glDepthFunc(depth);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
                glClearColor(clear[0], clear[1], clear[2], clear[3]);
#ifdef __ANDROID__
                glClearDepthf(clearDepth);
#else
                glClearDepth(clearDepth);
#endif
            }
        } cleanup{ oldRead,     oldDraw,       oldTexture,   oldRb,     oldPack, oldAlignment, oldRow,
                   oldSkipRows, oldSkipPixels, oldDepthFunc, oldUnpack, clear,   clearDepth };
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#ifdef __ANDROID__
        glClearDepthf(1);
#else
        glClearDepth(1);
#endif
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        bool colors = true, depth = true, state = true;
        unsigned cases = 0;
        const GLenum prior = glGetError();
        const auto savedLens = binocularLens;
        const bool savedLensActive = binocularLensActive;
        struct RestoreLens {
            LensPolygon lens;
            bool active;
            ~RestoreLens() {
                binocularLens = lens;
                binocularLensActive = active;
            }
        } restoreLens{ savedLens, savedLensActive };
        for (int optical = 0; optical < 3; ++optical) {
            ClearBinocularLens();
            if (optical) {
                XrPosef head{ { 0, 0, 0, 1 }, { 0, 0, 0 } }, eye = head;
                eye.position.x = optical == 1 ? -.032f : .032f;
                eye.position.y = .012f;
                eye.orientation = { 0, std::sin(optical == 1 ? .06f : -.06f), 0, std::cos(.06f) };
                SetBinocularLens({ -.95f, .65f, .60f, -.9f }, head, eye);
            }
            for (auto size : { std::array<int, 2>{ 64, 48 }, std::array<int, 2>{ 97, 73 } })
                for (int samples : { 1, 2 }) {
                    GLuint fb = 0, tex = 0, rb = 0, resolve = 0, msColor = 0;
                    glGenFramebuffers(1, &fb);
                    glGenFramebuffers(1, &resolve);
                    glGenTextures(1, &tex);
                    glGenRenderbuffers(1, &rb);
                    glGenRenderbuffers(1, &msColor);
                    struct Target {
                        GLuint f, t, r, resolve, ms;
                        ~Target() {
                            glDeleteFramebuffers(1, &f);
                            glDeleteFramebuffers(1, &resolve);
                            glDeleteTextures(1, &t);
                            glDeleteRenderbuffers(1, &r);
                            glDeleteRenderbuffers(1, &ms);
                        }
                    } target{ fb, tex, rb, resolve, msColor };
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    glBindFramebuffer(GL_FRAMEBUFFER, resolve);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                        throw std::runtime_error("Lens test resolve target incomplete");
                    glBindFramebuffer(GL_FRAMEBUFFER, fb);
                    if (samples > 1) {
                        glBindRenderbuffer(GL_RENDERBUFFER, msColor);
                        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, size[0], size[1]);
                        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
                    } else
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
                    glBindRenderbuffer(GL_RENDERBUFFER, rb);
                    if (samples > 1)
                        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, size[0],
                                                         size[1]);
                    else
                        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size[0], size[1]);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                        throw std::runtime_error("Lens test framebuffer incomplete");
                    auto readPixels = [&](int x, int y, int w, int h, unsigned char* pixels) {
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
                        if (samples > 1) {
                            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve);
                            glBlitFramebuffer(0, 0, size[0], size[1], 0, 0, size[0], size[1], GL_COLOR_BUFFER_BIT,
                                              GL_NEAREST);
                            glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve);
                        }
                        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                        glBindFramebuffer(GL_FRAMEBUFFER, fb);
                    };
                    for (int mode : { 1, 2 })
                        for (int offset : { 0, 1 }) {
                            ++cases;
                            for (GLenum cap : LensGlState::caps)
                                glDisable(cap);
                            glColorMask(1, 1, 1, 1);
                            glDepthMask(1);
                            glClearColor(0, 0, 1, 1);
                            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                            glEnable(GL_DEPTH_TEST);
                            glDepthFunc(GL_LESS);
                            int x = offset ? 5 : 0, y = offset ? 3 : 0, w = size[0] - 2 * x, h = size[1] - 2 * y;
                            glViewport(x, y, w, h);
                            Prepare(size[0], size[1]);
                            GLint vp[4]{};
                            glGetIntegerv(GL_VIEWPORT, vp);
                            state &= vp[0] == x && vp[1] == y && vp[2] == w && vp[3] == h && glIsEnabled(GL_DEPTH_TEST);
                            Apply(mode);
                            Draw(false, .25f, { 1, 0, 0, 1 });
                            std::vector<unsigned char> pixels(size[0] * size[1] * 4);
                            readPixels(0, 0, size[0], size[1], pixels.data());
                            auto examine = [&](bool after) {
                                for (int gy = 1; gy < 10; ++gy)
                                    for (int gx = 1; gx < 10; ++gx) {
                                        int px = int(gx * .1f * size[0]), py = int(gy * .1f * size[1]);
                                        const float ux = (px + .5f) / size[0], uy = (py + .5f) / size[1];
                                        // Ignore MSAA/polygon boundary samples, not interior mismatches.
                                        bool boundary = false;
                                        for (float dx : { -2.f, 2.f })
                                            for (float dy : { -2.f, 2.f })
                                                boundary |= LensInside(ux + dx / size[0], uy + dy / size[1]) !=
                                                            LensInside(ux, uy);
                                        if (boundary)
                                            continue;
                                        bool inside = LensInside((px + .5f) / size[0], (py + .5f) / size[1]);
                                        bool visible = mode == 1 ? inside : !inside;
                                        auto* c = &pixels[(py * size[0] + px) * 4];
                                        bool ok = visible ? (c[0] == 255 && c[1] == 0 && c[2] == 0)
                                                          : (after ? (c[0] == 0 && c[1] == 255 && c[2] == 0)
                                                                   : (c[0] == 0 && c[1] == 0 && c[2] == 255));
                                        (after ? depth : colors) &= ok;
                                    }
                            };
                            examine(false);
                            Apply(0);
                            Draw(false, .5f, { 0, 1, 0, 1 });
                            readPixels(0, 0, size[0], size[1], pixels.data());
                            examine(true);
                            // A nearer ordinary surface occludes lens geometry using native depth.
                            Draw(false, .1f, { 1, 1, 1, 1 });
                            Prepare(size[0], size[1]);
                            Apply(mode);
                            Draw(false, .2f, { 1, 0, 0, 1 });
                            readPixels(size[0] / 2, size[1] / 2, 1, 1, pixels.data());
                            depth &= pixels[0] == 255 && pixels[1] == 255 && pixels[2] == 255;
                            Apply(0);
                            state &= !glIsEnabled(GL_STENCIL_TEST);
                        }
                }
        } // optical configurations
        GLenum error = glGetError();
        std::ofstream out("native-lens-aperture.json");
        out << "{\"backend\":\"GLES\",\"msaa\":true,\"cases\":" << cases << ",\"color\":" << colors
            << ",\"depth\":" << depth << ",\"state\":" << state << ",\"error\":" << error << ",\"priorError\":" << prior
            << ",\"session\":\"" << (std::getenv("MMVR_SESSION_TOKEN") ? std::getenv("MMVR_SESSION_TOKEN") : "")
            << "\"}";
        out.close();
        if (!colors || !depth || !state || error || prior)
            throw std::runtime_error("Lens aperture render verification failed");
    }
};
} // namespace mmvr
