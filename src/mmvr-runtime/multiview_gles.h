#pragma once
#ifdef __ANDROID__
#include "gles_bridge.h"
#include <string>
#include <cstdlib>
#include <cstring>
namespace mmvr {
// One two-layer target, with independent depth for each eye. Existing per-eye
// postprocessing consumes extracted layers without changing its effect semantics.
class MultiviewTarget {
    using Attach = void(GL_APIENTRY*)(GLenum,GLenum,GLuint,GLint,GLint,GLsizei);
    Attach attach = nullptr;
    GLuint color=0, depth=0, fbo=0, readFbo=0;
    unsigned width=0,height=0;
    bool initialized=false;
    EGLContext owner=EGL_NO_CONTEXT;
    GlTarget layers[2];
public:
    bool Available() {
        const auto current=eglGetCurrentContext();
        if(current==EGL_NO_CONTEXT)return false;
        if(owner!=current){Reset();initialized=false;attach=nullptr;owner=current;}
        if(initialized) return attach != nullptr;
        initialized=true; owner=eglGetCurrentContext();
        if(std::getenv("MMVR_DISABLE_MULTIVIEW")) return false;
        GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);
        for(GLint i=0;i<count;++i)
            if(std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,i)),"GL_OVR_multiview2")==0)
                attach=reinterpret_cast<Attach>(eglGetProcAddress("glFramebufferTextureMultiviewOVR"));
        return attach != nullptr;
    }
    void Reset() {
        if(owner!=EGL_NO_CONTEXT && owner==eglGetCurrentContext()) {
            if(fbo)glDeleteFramebuffers(1,&fbo);
            if(readFbo)glDeleteFramebuffers(1,&readFbo);
            if(color)glDeleteTextures(1,&color);
            if(depth)glDeleteTextures(1,&depth);
        }
        for(auto& layer:layers)layer.Reset();
        fbo=readFbo=color=depth=0;width=height=0;
    }
    ~MultiviewTarget(){Reset();}
    bool Prepare(unsigned w,unsigned h,bool inverted) {
        if(!Available())return false;
        if(width==w && height==h && fbo) {
            for(auto& layer:layers) layer.image.inverted=inverted;
            return true;
        }
        GlFramebufferState saved;
        GLint previous=0;glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&previous);
        Reset();width=w;height=h;
        auto storage=[&](GLuint& texture,GLenum format){
            glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,format,w,h,2);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        };
        storage(color,GL_RGBA8);storage(depth,GL_DEPTH24_STENCIL8);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,color,0,0,2);
        attach(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,depth,0,0,2);
        const bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        glBindTexture(GL_TEXTURE_2D_ARRAY,previous);
        if(!complete){Reset();attach=nullptr;return false;}
        glGenFramebuffers(1,&readFbo);
        for(auto& layer:layers)layer.Resize(w,h,inverted);
        return true;
    }
    GLuint Framebuffer()const{return fbo;}
    // Expose one layered color attachment as a framebuffer-only image. The XR
    // transfer path already handles orientation and encoded color from an FBO;
    // ordinary eye frames need no full-size 2D extraction texture.
    GlImage ReadLayer(int eye) {
        GlFramebufferState saved;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,color,0,eye);
        return {0,readFbo,width,height,1,layers[eye].image.inverted};
    }
    GlImage* Extract(int eye) {
        GlFramebufferState saved;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,color,0,eye);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,layers[eye].image.framebuffer);
        glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        return &layers[eye].image;
    }
};
}
#endif

