#pragma once
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <functional>
#include <stdexcept>
#include <cstring>
#include <array>
#include <algorithm>
namespace mmvr {
struct GlDevice {};
struct GlContext {};
// inverted means the game's top row is stored at GL's bottom edge.
struct GlImage {
    GLuint texture = 0, framebuffer = 0;
    unsigned width = 0, height = 0, samples = 1;
    bool inverted = false;
};
struct GlFramebufferState {
    GLint read = 0, draw = 0;
    GLboolean scissor;
    GlFramebufferState() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
    }
    ~GlFramebufferState() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        if (scissor)
            glEnable(GL_SCISSOR_TEST);
    }
};
struct GlTarget {
    GlImage image;
    GLenum storageFormat = 0;
    EGLContext owner = EGL_NO_CONTEXT;
    GlTarget() = default;
    GlTarget(const GlTarget&) = delete;
    GlTarget& operator=(const GlTarget&) = delete;
    ~GlTarget() {
        Reset();
    }
    void Reset() {
        // Names may be reused by a replacement context. Never delete those
        // unrelated objects after the owning context has gone away.
        if (owner != EGL_NO_CONTEXT && owner == eglGetCurrentContext()) {
            if (image.framebuffer) glDeleteFramebuffers(1, &image.framebuffer);
            if (image.texture) glDeleteTextures(1, &image.texture);
        }
        image = {};
        storageFormat = 0;
        owner = EGL_NO_CONTEXT;
    }
    void Resize(unsigned w, unsigned h, bool inverted = false, GLenum format = GL_RGBA8) {
        const auto current = eglGetCurrentContext();
        if (current == EGL_NO_CONTEXT) throw std::runtime_error("GLES target requires a current context");
        if (owner == current && image.texture && image.framebuffer &&
            image.width == w && image.height == h && storageFormat == format) {
            image.inverted = inverted;
            return;
        }
        GlFramebufferState save;
        GLint texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        // Deleting a currently bound old target unbinds it. Do not recreate its
        // stale numeric name when restoring GL state after resize.
        if (owner == current && GLuint(texture) == image.texture) texture = 0;
        Reset();
        owner = current;
        image.width = w;
        image.height = h;
        image.inverted = inverted;
        storageFormat = format;
        glGenTextures(1, &image.texture);
        glBindTexture(GL_TEXTURE_2D, image.texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, format, w, h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &image.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, image.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image.texture, 0);
        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindTexture(GL_TEXTURE_2D, texture);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete GLES VR render target");
    }
};
inline void GlBlit(const GlImage& source, const GlImage& target) {
    GlFramebufferState save;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.framebuffer);
    const bool flip = source.inverted != target.inverted;
    glBlitFramebuffer(0, 0, source.width, source.height, 0, flip ? target.height : 0, target.width,
                      flip ? 0 : target.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}
class GlTransfer {
    GlTarget resolve, oriented;
    GLuint destination = 0;
    EGLContext owner = EGL_NO_CONTEXT;
    // OpenXR swapchain textures are immutable while their chain lives. Their
    // completeness is checked once per image and forgotten when a chain is
    // recreated; repeatedly querying the driver here stalls eye submission.
    std::array<GLuint, 32> validatedImages{};
    size_t validatedImageCount = 0;
    using CopyImageFn = void(GL_APIENTRY*)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint,
                                           GLint, GLint, GLsizei, GLsizei, GLsizei);
    CopyImageFn copyImage = nullptr;
    bool transferInitialized = false, writeControl = false, preferDirect = true;
    void Init() {
        const auto current = eglGetCurrentContext();
        if (current == EGL_NO_CONTEXT) throw std::runtime_error("GLES transfer requires a current context");
        if (owner != current) {
            // Old-context allocations are not names in the replacement context.
            destination = 0;
            resolve.Reset(); oriented.Reset();
            transferInitialized = false; writeControl = false; copyImage = nullptr;
            validatedImageCount = 0;
            owner = current;
        }
        if (transferInitialized)
            return;
        transferInitialized = true;
        GLint count = 0, major = 0, minor = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        bool extCopy = false, oesCopy = false;
        for (GLint i = 0; i < count; ++i) {
            const char* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            extCopy |= std::strcmp(name, "GL_EXT_copy_image") == 0;
            oesCopy |= std::strcmp(name, "GL_OES_copy_image") == 0;
            writeControl |= std::strcmp(name, "GL_EXT_sRGB_write_control") == 0;
        }
        const char* entry = major > 3 || (major == 3 && minor >= 2) ? "glCopyImageSubData"
                            : extCopy                               ? "glCopyImageSubDataEXT"
                            : oesCopy                               ? "glCopyImageSubDataOES"
                                                                    : nullptr;
        if (entry)
            copyImage = reinterpret_cast<CopyImageFn>(eglGetProcAddress(entry));
        if (!copyImage && !writeControl)
            throw std::runtime_error("Color-preserving XR transfer requires copy_image or sRGB_write_control");
    }

  public:
    explicit GlTransfer(bool direct = true) : preferDirect(direct) {
    }
    ~GlTransfer() {
        if (destination && owner != EGL_NO_CONTEXT && owner == eglGetCurrentContext())
            glDeleteFramebuffers(1, &destination);
    }
    void ForgetSwapchainImages() { validatedImageCount = 0; }
    void Copy(const GlImage& from, const GlImage& to) {
        if (from.samples > 1) {
            resolve.Resize(from.width, from.height, from.inverted);
            GlBlit(from, resolve.image);
            GlBlit(resolve.image, to);
        } else
            GlBlit(from, to);
    }
    void ToSwapchain(const GlImage& source, GLuint image) {
        Init();
        GlFramebufferState save;
        // Fast3D produces display-encoded pixels. Copy compatible-format bits into
        // SRGB8_ALPHA8; an ordinary linear-to-sRGB blit would encode them twice.
        // A flipped source otherwise needs a full-size intermediate plus a second copy.
        // EXT_sRGB_write_control lets one blit preserve the same encoded RGBA bytes.
        if (copyImage && !(preferDirect && writeControl && (source.inverted || !source.texture))) {
            const GlImage* from = &source;
            if (source.inverted || source.samples > 1 || !source.texture) {
                oriented.Resize(source.width, source.height, false);
                Copy(source, oriented.image);
                from = &oriented.image;
            }
            copyImage(from->texture, GL_TEXTURE_2D, 0, 0, 0, 0, image, GL_TEXTURE_2D, 0, 0, 0, 0, source.width,
                      source.height, 1);
        } else {
            if (!destination)
                glGenFramebuffers(1, &destination);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
            const bool validated = std::find(validatedImages.begin(),
                                             validatedImages.begin() + validatedImageCount, image) !=
                                   validatedImages.begin() + validatedImageCount;
            if (!validated) {
                if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("Incomplete OpenXR GLES image");
                if (validatedImageCount < validatedImages.size())
                    validatedImages[validatedImageCount++] = image;
            }
            constexpr GLenum srgbWrite = 0x8DB9;
            const bool wasEnabled = glIsEnabled(srgbWrite);
            glDisable(srgbWrite);
            Copy(source, { image, destination, source.width, source.height, 1, false });
            if (wasEnabled)
                glEnable(srgbWrite);
        }
    }
};
inline void GlVerifyTransfer(bool direct = true) {
    GlFramebufferState saved;
    GLint binding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    GlTarget input, output;
    input.Resize(2, 2);
    output.Resize(2, 2, false, GL_SRGB8_ALPHA8);
    const std::array<unsigned char, 16> pixels{ 19, 91, 173, 33, 237, 127, 41, 255, 83, 197, 29, 0, 9, 57, 209, 137 };
    glBindTexture(GL_TEXTURE_2D, input.image.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, binding);
    GlTransfer transfer(direct);
    for (bool inverted : { false, true }) {
        input.image.inverted = inverted;
        transfer.ToSwapchain(input.image, output.image.texture);
        if (inverted) {
            transfer.ToSwapchain(input.image, output.image.texture); // cached destination
            transfer.ForgetSwapchainImages();
            transfer.ToSwapchain(input.image, output.image.texture); // recreated chain path
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output.image.framebuffer);
        std::array<unsigned char, 16> actual{};
        glReadPixels(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        for (size_t i = 0; i < actual.size(); ++i) {
            size_t expected = inverted ? (i + 8) % 16 : i;
            if (actual[i] != pixels[expected])
                throw std::runtime_error("GLES XR color/alpha/orientation self-test failed");
        }
    }
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("GLES error during XR image transfer self-test");
}
void SubmitGameGLES(GlImage&, const std::function<void(bool)>&) noexcept;
bool PostNativeOrderingPending() noexcept;
void VerifyPostNativeOrderingGLES(GlImage&,const std::function<void(bool)>&);
} // namespace mmvr
#endif
