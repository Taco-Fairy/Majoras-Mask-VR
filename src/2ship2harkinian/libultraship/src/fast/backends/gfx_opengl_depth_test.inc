// Actual engine depth-state path, with pixels produced by the active GLES GPU.
// The small shader isolates depth comparison from native material/resource setup.
void GfxRenderingAPIOGL::VerifyDepthComparison() {
    mmvr::LensGlState state;
    mmvr::GlFramebufferState framebuffer;
    GLint oldFunction=0,oldRenderbuffer=0,oldPack=0,pack[4]{};
    GLfloat oldClearDepth=1,oldRange[2]{},oldColor[4]{};
    glGetIntegerv(GL_DEPTH_FUNC,&oldFunction);glGetIntegerv(GL_RENDERBUFFER_BINDING,&oldRenderbuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&oldPack);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE,&oldClearDepth);glGetFloatv(GL_DEPTH_RANGE,oldRange);glGetFloatv(GL_COLOR_CLEAR_VALUE,oldColor);
    const GLenum packNames[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS};
    for(int i=0;i<4;++i){glGetIntegerv(packNames[i],&pack[i]);glPixelStorei(packNames[i],i?0:1);}
    const std::array<int8_t,6> cached{mCurrentDepthTest,mCurrentDepthMask,mCurrentZmodeDecal,mLastDepthTest,mLastDepthMask,mLastZmodeDecal};
    GLuint program=0,vao=0,depth=0;
    auto cleanup=[&](){
        if(program)glDeleteProgram(program);if(vao)glDeleteVertexArrays(1,&vao);if(depth)glDeleteRenderbuffers(1,&depth);
        mCurrentDepthTest=cached[0];mCurrentDepthMask=cached[1];mCurrentZmodeDecal=cached[2];
        mLastDepthTest=cached[3];mLastDepthMask=cached[4];mLastZmodeDecal=cached[5];
        glDepthFunc(oldFunction);glClearDepthf(oldClearDepth);glDepthRangef(oldRange[0],oldRange[1]);
        glClearColor(oldColor[0],oldColor[1],oldColor[2],oldColor[3]);
        glBindRenderbuffer(GL_RENDERBUFFER,oldRenderbuffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPack);
        for(int i=0;i<4;++i)glPixelStorei(packNames[i],pack[i]);
    };
    try {
        mmvr::GlTarget target;target.Resize(8,8);
        glBindFramebuffer(GL_FRAMEBUFFER,target.image.framebuffer);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,8,8);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Depth probe framebuffer incomplete");
        auto compile=[](GLenum type,const char* source){
            GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
            GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
            if(!ok){glDeleteShader(shader);throw std::runtime_error("Depth probe shader failed");}return shader;
        };
        GLuint vertex=compile(GL_VERTEX_SHADER,"#version 300 es\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],-1,1);}");
        GLuint fragment=0;
        try{fragment=compile(GL_FRAGMENT_SHADER,"#version 300 es\nprecision highp float;out vec4 result;void main(){result=vec4(1,0,0,1);}");}
        catch(...){glDeleteShader(vertex);throw;}
        program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);
        glDeleteShader(vertex);glDeleteShader(fragment);
        GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Depth probe link failed");
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glUseProgram(program);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glViewport(0,0,8,8);glDepthRangef(0,1);
        for(auto cap:mmvr::LensGlState::caps)glDisable(cap);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        bool pixels=true,comparison=true;unsigned cases=0;
        for(bool test:{false,true})for(bool write:{false,true})for(bool decal:{false,true,false,true,true,false}) {
            glDepthMask(GL_TRUE);glClearDepthf(0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            // Restore the cached write state after clearing; production clears
            // likewise preserve their caller's depth mask.
            glDepthMask(mLastDepthMask>0?GL_TRUE:GL_FALSE);
            mCurrentDepthTest=test;mCurrentDepthMask=write;mCurrentZmodeDecal=decal;
            ApplyDepthState();
            mLastZmodeDecal=mCurrentZmodeDecal; // DrawTriangles commits this after polygon-offset setup.
            GLint function=0;glGetIntegerv(GL_DEPTH_FUNC,&function);
            comparison&=bool(glIsEnabled(GL_DEPTH_TEST))==(test||write);
            if(test||write)comparison&=function==(test?(decal?GL_LEQUAL:GL_LESS):GL_ALWAYS);
            glDrawArrays(GL_TRIANGLES,0,3);
            unsigned char pixel[4]{};glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            pixels&=pixel[0]==((!test||decal)?255:0)&&pixel[1]==0&&pixel[2]==0&&pixel[3]==255;
            ++cases;
        }
        const auto error=glGetError();
        std::ofstream("native-depth-comparison.json")<<"{\"passed\":"<<(pixels&&comparison&&error==GL_NO_ERROR?"true":"false")<<",\"cases\":"<<cases<<",\"pixels\":"<<pixels<<",\"comparison\":"<<comparison<<",\"glError\":"<<error<<"}";
        if(!pixels||!comparison||error!=GL_NO_ERROR)throw std::runtime_error("Native GLES depth comparison regression");
        cleanup();
    } catch(...) {cleanup();throw;}
}
