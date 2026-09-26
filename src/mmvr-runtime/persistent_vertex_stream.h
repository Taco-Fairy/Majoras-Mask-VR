#pragma once
// GLES-only streaming storage. Ranges are never reused until their GPU fence signals.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>
namespace mmvr {
class PersistentVertexStream {
  public:
    using StorageFn = void(GL_APIENTRYP)(GLenum, GLsizeiptr, const void*, GLbitfield);
    struct Slice {
        GLuint buffer = 0;
        GLint first = 0;
    };
    bool Init(StorageFn storage, size_t regionBytes = 4 * 1024 * 1024) {
        if (buffer || !storage || regionBytes < 256)
            return false;
        bool extension = false;
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i) {
            auto name = (const char*)glGetStringi(GL_EXTENSIONS, i);
            extension |= name && std::string_view(name) == "GL_EXT_buffer_storage";
        }
        if (!extension)
            return false;
        GLint previous = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
        context = eglGetCurrentContext();
        regionSize = regionBytes;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        constexpr GLbitfield flags = GL_MAP_WRITE_BIT | 0x0040 /*PERSISTENT*/ | 0x0080 /*COHERENT*/;
        storage(GL_ARRAY_BUFFER, GLsizeiptr(regionSize * regions.size()), nullptr, flags);
        mapped = (unsigned char*)glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(regionSize * regions.size()), flags);
        if (!mapped) {
            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }
        glBindBuffer(GL_ARRAY_BUFFER, GLuint(previous));
        enabled = mapped != nullptr;
        return enabled;
    }
    bool Upload(const void* data, size_t bytes, size_t stride, Slice& out) {
        if (!enabled || eglGetCurrentContext() != context || !data || !bytes || !stride || bytes % stride ||
            bytes > regionSize)
            return false;
        auto aligned = [&]() { return ((cursor + stride - 1) / stride) * stride; };
        size_t offset = aligned();
        // Once sealed, a region must not accept smaller later batches after its fence.
        // Otherwise that fence could signal before those later draws finish.
        if (regions[current] || offset + bytes > (current + 1) * regionSize) {
            // Fence after the last draw that consumed this region. A busy ring uses the
            // renderer's ordinary buffer for this batch; never wait on the render thread.
            if (!regions[current]) {
                regions[current] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                if (!regions[current]) {
                    enabled = false;
                    return false;
                }
                ++fences;
            }
            bool found = false;
            for (size_t step = 1; step <= regions.size(); ++step) {
                size_t next = (current + step) % regions.size();
                if (regions[next]) {
                    GLenum status = glClientWaitSync(regions[next], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
                    if (status == GL_WAIT_FAILED) {
                        enabled = false;
                        return false;
                    }
                    if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
                        continue;
                    glDeleteSync(regions[next]);
                    regions[next] = nullptr;
                }
                const size_t candidate = ((next * regionSize + stride - 1) / stride) * stride;
                if (candidate + bytes > (next + 1) * regionSize)
                    continue;
                current = next;
                cursor = current * regionSize;
                offset = candidate;
                found = true;
                break;
            }
            if (!found) {
                ++fallbacks;
                return false;
            }
        }
        memcpy(mapped + offset, data, bytes);
        cursor = offset + bytes;
        out = { buffer, GLint(offset / stride) };
        ++uploads;
        return true;
    }
    ~PersistentVertexStream() {
        // If SDL already destroyed the context, its allocations are already gone.
        if (buffer && eglGetCurrentContext() == context) {
            for (auto fence : regions)
                if (fence)
                    glDeleteSync(fence);
            glDeleteBuffers(1, &buffer);
        }
    }
    PersistentVertexStream() = default;
    PersistentVertexStream(const PersistentVertexStream&) = delete;
    PersistentVertexStream& operator=(const PersistentVertexStream&) = delete;
    size_t uploads = 0, fences = 0, fallbacks = 0;

  private:
    EGLContext context = EGL_NO_CONTEXT;
    GLuint buffer = 0;
    unsigned char* mapped = nullptr;
    std::array<GLsync, 4> regions{};
    size_t regionSize = 0, current = 0, cursor = 0;
    bool enabled = false;
};
} // namespace mmvr
