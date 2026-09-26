#pragma once
#ifdef __ANDROID__
#include <fast/backends/gfx_opengl.h>
#include <set>
static mmvr::Pad NativeShaderLifetime(unsigned tick) {
    static std::ofstream log("native-shader-lifecycle.log");
    if(tick==1)if(const char* token=std::getenv("MMVR_SESSION_TOKEN"))log<<"session "<<token<<"\n"<<std::flush;
    mmvr::Pad pad;pad.active=true;
    if(tick==20) {
        auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
        dynamic_cast<Fast::GfxRenderingAPIOGL*>(window->GetInterpreterWeak().lock()->mRapi)->VerifyFramebufferCopies();
    }
    if(tick==25){
        auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
        dynamic_cast<Fast::GfxRenderingAPIOGL*>(window->GetInterpreterWeak().lock()->mRapi)->VerifyDrawStateCaching();
    }
    if(tick==30) NativeResourcePerformance(); // Actual GLES/ARM64 resource decoder and pack replacement.
    if(tick==40||tick==80||tick==120) {
        const GLenum priorError=glGetError();
        auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
        auto interpreter=window->GetInterpreterWeak().lock();
        std::set<GLuint> programs,arrays;
        for(const auto& entry:interpreter->mColorCombinerPool)
            for(auto* program:entry.second.prg)if(program){programs.insert(program->openglProgramId);for(GLuint vao:program->vertexArrays)if(vao)arrays.insert(vao);}
        bool detached=!programs.empty();
        for(GLuint program:programs){GLint count=0;glGetProgramiv(program,GL_ATTACHED_SHADERS,&count);detached&=count==0;}
        gfx_shader_cache_clear();
        bool released=true;for(GLuint program:programs)released&=!glIsProgram(program);
        bool arraysReleased=true;for(GLuint vao:arrays)arraysReleased&=!glIsVertexArray(vao);
        const GLenum error=glGetError();
        log<<"cycle="<<tick/40<<" programs="<<programs.size()<<" stagesReleased="<<detached<<" programsReleased="<<released<<" arrays="<<arrays.size()<<" arraysReleased="<<arraysReleased<<" priorGlError="<<priorError<<" glError="<<error<<"\n"<<std::flush;
        if(!detached||!released||!arraysReleased||priorError!=GL_NO_ERROR||error!=GL_NO_ERROR){log<<"ERROR shader cleanup failed\n"<<std::flush;window->Close();}
    }
    if(tick==140){log<<"COMPLETE\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
    return pad;
}
#endif
