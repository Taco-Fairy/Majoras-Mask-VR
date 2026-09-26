#pragma once
#include <cstdint>
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstring>
#endif
namespace mmvr {
// Asynchronous timestamps: never wait for the GPU or flush to obtain a sample.
struct GpuTiming {
    double sum = 0;
    unsigned samples = 0;
    double lastMs = 0;
#ifdef __ANDROID__
    using BeginFn = void(GL_APIENTRY*)(GLenum, GLuint);
    using EndFn = void(GL_APIENTRY*)(GLenum);
    using ReadFn = void(GL_APIENTRY*)(GLuint, GLenum, GLuint64*);
    using AvailFn = void(GL_APIENTRY*)(GLuint, GLenum, GLuint*);
    BeginFn begin = nullptr;
    EndFn end = nullptr;
    ReadFn read = nullptr;
    AvailFn available = nullptr;
    std::array<GLuint, 8> queries{};
    std::array<bool, 8> pending{};
    std::array<bool, 8> invalid{};
    unsigned next = 0, frame = 0;
    int active = -1;
    bool initialized = false, supported = false;
    EGLContext owner = EGL_NO_CONTEXT;
    void ResetContext() {
        if (owner != EGL_NO_CONTEXT && owner == eglGetCurrentContext()) {
            if (active >= 0 && end) end(0x88BF);
            if (supported) glDeleteQueries(queries.size(), queries.data());
        }
        // Names and extension entry points belong to one context. Never issue
        // calls against an unrelated context after loss/recreation.
        owner = EGL_NO_CONTEXT;
        queries.fill(0); pending.fill(false); invalid.fill(false);
        begin = nullptr; end = nullptr; read = nullptr; available = nullptr;
        next = frame = 0; active = -1;
        initialized = supported = false;
        sum = lastMs = 0; samples = 0;
    }
    void Init() {
        const auto current = eglGetCurrentContext();
        if (owner != current) ResetContext();
        if (current == EGL_NO_CONTEXT) return;
        owner = current;
        if (initialized)
            return;
        initialized = true;
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (int i = 0; i < count; ++i)
            supported |= !std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)),
                                      "GL_EXT_disjoint_timer_query");
        if (!supported)
            return;
        begin = reinterpret_cast<BeginFn>(eglGetProcAddress("glBeginQueryEXT"));
        end = reinterpret_cast<EndFn>(eglGetProcAddress("glEndQueryEXT"));
        read = reinterpret_cast<ReadFn>(eglGetProcAddress("glGetQueryObjectui64vEXT"));
        available = reinterpret_cast<AvailFn>(eglGetProcAddress("glGetQueryObjectuivEXT"));
        supported = begin && end && read && available;
        if (supported)
            glGenQueries(queries.size(), queries.data());
    }
    ~GpuTiming() { ResetContext(); }
    void Begin() {
        Init();
        if (!supported || active >= 0)
            return;
        // Sparse sampling avoids polling the driver for every eye submission.
        // Twenty samples per five seconds at 120 Hz still identify GPU-bound scenes.
        if ((frame++ % 30) != 0)
            return;
        GLint disjoint = 0;
        glGetIntegerv(0x8FBB, &disjoint);
        // A disjoint also invalidates unfinished queries. Keep that fact until
        // each result arrives; availability can lag the clock discontinuity.
        if (disjoint)
            for (unsigned i = 0; i < pending.size(); ++i)
                if (pending[i]) invalid[i] = true;
        for (int i = 0; i < 8; ++i)
            if (pending[i]) {
                GLuint ready = 0;
                available(queries[i], GL_QUERY_RESULT_AVAILABLE, &ready);
                if (ready) {
                    GLuint64 ns = 0;
                    read(queries[i], GL_QUERY_RESULT, &ns);
                    pending[i] = false;
                    if (!invalid[i] && ns > 0 && ns < 1000000000) {
                        lastMs = double(ns) / 1e6;
                        sum += lastMs;
                        ++samples;
                    }
                }
            }
        if (disjoint) {
            sum = 0;
            samples = 0;
            lastMs = 0;
            return;
        }
        if (pending[next])
            return;
        active = next;
        invalid[next] = false;
        begin(0x88BF, queries[next]);
        next = (next + 1) % 8;
    }
    void End() {
        if (owner != eglGetCurrentContext()) { ResetContext(); return; }
        if (active < 0)
            return;
        end(0x88BF);
        pending[active] = true;
        active = -1;
    }
#else
    void Begin() {
    }
    void End() {
    }
#endif
    double Mean() const {
        return samples ? sum / samples : 0;
    }
    void ResetWindow() {
        sum = 0;
        samples = 0;
    }
};
} // namespace mmvr


