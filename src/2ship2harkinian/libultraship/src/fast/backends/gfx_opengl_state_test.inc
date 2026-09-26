// Included inside Fast, only for the Android VR backend. Runs exclusively in
// the protected native GLES diagnostic. Its scratch buffers never replace the
// contents or attribute layout of the live game's ordinary buffer/VAO.
void GfxRenderingAPIOGL::VerifyDrawStateCaching() {
    if(!std::getenv("MMVR_SHADER_TEST"))return;
    GLint oldProgram,oldVao,oldBuffer,oldRead,oldDraw,oldViewport[4],oldPack;
    glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&oldRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldDraw);glGetIntegerv(GL_VIEWPORT,oldViewport);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&oldPack);
    const std::array<GLenum,4> packNames{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS};
    GLint packValues[4];for(size_t i=0;i<4;++i)glGetIntegerv(packNames[i],&packValues[i]);
    const std::array<GLenum,9> caps{GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_POLYGON_OFFSET_FILL,
        GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
    bool capValues[9];for(size_t i=0;i<caps.size();++i)capValues[i]=glIsEnabled(caps[i]);
    GLboolean colorMask[4],depthMask;GLfloat clearColor[4];
    glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColor);
    auto* savedProgram=mCurrentShaderProgram;auto* savedLast=mLastLoadedShader;
    const auto savedBuffer=mActiveVertexBuffer,savedOrdinary=mOpenglVbo,savedFrame=mFrameCount;
    const auto savedStreamCursor=mStreamCursor,savedStreamCapacity=mStreamCapacity;
    const auto savedNoise=mCurrentNoiseScale,savedPrim=mCurrentPrimDepth;
    auto savedTextures=textures;std::array<GLuint,SHADER_MAX_TEXTURES> savedIds;
    std::copy(std::begin(mCurrentTextureIds),std::end(mCurrentTextureIds),savedIds.begin());
    const std::array<int8_t,6> depthState{mCurrentDepthTest,mCurrentDepthMask,mCurrentZmodeDecal,mLastDepthTest,mLastDepthMask,mLastZmodeDecal};
    ShaderProgram probes[2]{};GLuint ordinary=0,referenceVao=0,referenceBuffer=0;
    mmvr::GlTarget target;
    bool restored=false;
    auto restore=[&](){
        if(restored)return;
        restored=true;
        mCurrentShaderProgram=savedProgram;mLastLoadedShader=savedLast;mActiveVertexBuffer=savedBuffer;
        mOpenglVbo=savedOrdinary;mStreamCursor=savedStreamCursor;mStreamCapacity=savedStreamCapacity;
        mFrameCount=savedFrame;mCurrentNoiseScale=savedNoise;mCurrentPrimDepth=savedPrim;
        textures=std::move(savedTextures);std::copy(savedIds.begin(),savedIds.end(),std::begin(mCurrentTextureIds));
        mCurrentDepthTest=depthState[0];mCurrentDepthMask=depthState[1];mCurrentZmodeDecal=depthState[2];
        mLastDepthTest=depthState[3];mLastDepthMask=depthState[4];mLastZmodeDecal=depthState[5];
        glUseProgram(oldProgram);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,oldRead);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,oldDraw);
        glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
        glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);glDepthMask(depthMask);
        glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);
        for(size_t i=0;i<caps.size();++i)if(capValues[i])glEnable(caps[i]);else glDisable(caps[i]);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPack);for(size_t i=0;i<4;++i)glPixelStorei(packNames[i],packValues[i]);
        for(auto& prg:probes){glDeleteVertexArrays(2,prg.vertexArrays);if(prg.openglProgramId)glDeleteProgram(prg.openglProgramId);}
        if(referenceVao)glDeleteVertexArrays(1,&referenceVao);
        if(referenceBuffer)glDeleteBuffers(1,&referenceBuffer);
        if(ordinary)glDeleteBuffers(1,&ordinary);
        target.Reset();
    };
    struct RestoreOnExit {std::function<void()> apply;~RestoreOnExit(){apply();}} restoreOnExit{restore};
    for(size_t i=0;i<4;++i)glPixelStorei(packNames[i],i?0:1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    for(auto cap:caps)glDisable(cap);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glViewport(0,0,16,16);
    mCurrentDepthTest=mCurrentDepthMask=mCurrentZmodeDecal=mLastDepthTest=mLastDepthMask=mLastZmodeDecal=0;
    mCurrentShaderProgram=mLastLoadedShader=nullptr;
    textures.resize(std::max(size_t(2),textures.size()));mCurrentTextureIds[0]=0;mCurrentTextureIds[1]=1;
    glGenBuffers(1,&ordinary);glGenBuffers(1,&referenceBuffer);glGenVertexArrays(1,&referenceVao);
    mOpenglVbo=mActiveVertexBuffer=ordinary;mStreamCursor=mStreamCapacity=0;
    glBindBuffer(GL_ARRAY_BUFFER,ordinary);
    target.Resize(16,16);glBindFramebuffer(GL_FRAMEBUFFER,target.image.framebuffer);
    auto compile=[](GLenum type,const std::string& source){
        GLuint result=glCreateShader(type);const char* text=source.c_str();glShaderSource(result,1,&text,nullptr);glCompileShader(result);
        GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
        if(!ok){glDeleteShader(result);throw std::runtime_error("Draw-state probe shader failed");}return result;
    };
    const std::string fragment=R"(#version 300 es
precision highp float;
in vec4 vColor;
uniform int frame_count;
uniform float noise_scale,prim_depth;
uniform int texture_width[2],texture_height[2],texture_filtering[2];
out vec4 result;
void main(){result=vec4(vColor.rgb*noise_scale+vec3(float(frame_count)*0.002,
float(texture_width[0]+texture_width[1]+texture_filtering[0]+texture_filtering[1])*0.001,
float(texture_height[0]+texture_height[1])*0.001+prim_depth*0.1),1.0);})";
    for(int i=0;i<2;++i){
        auto& prg=probes[i];
        std::string vertex="#version 300 es\nlayout(location="+std::to_string(i?1:0)+") in vec4 position;\nlayout(location="+std::to_string(i?4:2)+") in vec4 color;\nout vec4 vColor;void main(){gl_Position=position;vColor=color;}";
        GLuint vs=0,fs=0;
        try{
            vs=compile(GL_VERTEX_SHADER,vertex);fs=compile(GL_FRAGMENT_SHADER,fragment);
            prg.openglProgramId=glCreateProgram();glAttachShader(prg.openglProgramId,vs);glAttachShader(prg.openglProgramId,fs);glLinkProgram(prg.openglProgramId);
            GLint ok=0;glGetProgramiv(prg.openglProgramId,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Draw-state probe link failed");
            glDetachShader(prg.openglProgramId,vs);glDetachShader(prg.openglProgramId,fs);
        }catch(...){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);throw;}
        glDeleteShader(vs);glDeleteShader(fs);
        prg.numAttribs=i?3:2;prg.numFloats=i?10:8;
        prg.attribLocations[0]=i?1:0;prg.attribSizes[0]=4;
        if(i){prg.attribLocations[1]=-1;prg.attribSizes[1]=2;prg.attribLocations[2]=4;prg.attribSizes[2]=4;}
        else{prg.attribLocations[1]=2;prg.attribSizes[1]=4;}
        prg.usedTextures[0]=prg.usedTextures[1]=true;
        prg.frameCountLocation=glGetUniformLocation(prg.openglProgramId,"frame_count");
        prg.noiseScaleLocation=glGetUniformLocation(prg.openglProgramId,"noise_scale");
        prg.prim_depth_location=glGetUniformLocation(prg.openglProgramId,"prim_depth");
        prg.texture_width_location=glGetUniformLocation(prg.openglProgramId,"texture_width");
        prg.texture_height_location=glGetUniformLocation(prg.openglProgramId,"texture_height");
        prg.texture_filtering_location=glGetUniformLocation(prg.openglProgramId,"texture_filtering");
    }
    bool pixels=true,uniforms=true;unsigned cases=0,identical=0,frameOnly=0,noiseOnly=0,drawOnly=0;
    const auto uploads=mPersistentVertices.uploads;
    // The standard Quest diagnostic must exercise the persistent ring. Explicit
    // legacy/stream experiments may test fallback separately without claiming it.
    const bool requirePersistent=!std::getenv("MMVR_LEGACY_VERTEX_UPLOAD")&&!std::getenv("MMVR_STREAM_VERTEX_UPLOAD");
    for(int batch=0;batch<8;++batch){
        auto& prg=probes[batch%2];const bool persistentDraw=(batch/2)%2==0;
        mFrameCount=batch*3;mCurrentNoiseScale=.5f;mCurrentPrimDepth=(batch%3)*.2f;
        textures[0]={uint16_t(8+batch%4),uint16_t(16+batch%2),uint16_t(batch%3)};
        textures[1]={uint16_t(24+batch%2),uint16_t(32+batch%4),uint16_t((batch+1)%3)};
        const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};std::vector<float> vertices;
        for(auto& pos:xy){vertices.insert(vertices.end(),{pos[0],pos[1],0,1});if(batch%2)vertices.insert(vertices.end(),{123,456});vertices.insert(vertices.end(),{.2f,.3f,.4f,1});}
        if(mLastLoadedShader!=&prg){UnloadShader(mLastLoadedShader);LoadShader(&prg);}
        for(int step=0;step<5;++step){
            if(step==1)++identical;
            if(step==2){++mFrameCount;++frameOnly;}
            if(step==3){mCurrentNoiseScale=.75f;++noiseOnly;}
            if(step==4){mCurrentPrimDepth+=.1f;++textures[0].width;++textures[1].height;textures[1].filtering=(textures[1].filtering+1)%3;++drawOnly;}
            const GLint widths[]{textures[0].width,textures[1].width},heights[]{textures[0].height,textures[1].height},filtering[]{textures[0].filtering,textures[1].filtering};
            glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
            // No LoadShader between these five draws: changed frame/noise state
            // must reach the existing program through the per-draw cache path.
            if(persistentDraw)DrawTriangles(vertices.data(),vertices.size(),1);
            else{
                if(mActiveVertexBuffer!=mOpenglVbo)BindVertexLayout(&prg,mOpenglVbo);
                glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STREAM_DRAW);
                SetPerDrawUniforms();glDrawArrays(GL_TRIANGLES,0,3);
            }
            std::array<unsigned char,16*16*4> actual{},reference{};
            glReadPixels(0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
            GLint actualFrame=0;GLfloat actualNoise=0,actualPrim=0;
            glGetUniformiv(prg.openglProgramId,prg.frameCountLocation,&actualFrame);glGetUniformfv(prg.openglProgramId,prg.noiseScaleLocation,&actualNoise);glGetUniformfv(prg.openglProgramId,prg.prim_depth_location,&actualPrim);
            uniforms&=actualFrame==GLint(mFrameCount)&&actualNoise==mCurrentNoiseScale&&actualPrim==mCurrentPrimDepth;
            for(int t=0;t<2;++t)for(int k=0;k<3;++k){
                const char* names[]{"texture_width","texture_height","texture_filtering"};const GLint* expected[]{widths,heights,filtering};
                const std::string name=std::string(names[k])+"["+std::to_string(t)+"]";GLint value=0;
                glGetUniformiv(prg.openglProgramId,glGetUniformLocation(prg.openglProgramId,name.c_str()),&value);uniforms&=value==expected[k][t];
            }
            // Compare the exact pixels against the uncached GL layout/uniform
            // path, then restore the cached binding without unloading its shader.
            GLint cachedVao=0,cachedBuffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&cachedVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&cachedBuffer);
            glBindVertexArray(referenceVao);glBindBuffer(GL_ARRAY_BUFFER,referenceBuffer);
            for(int attrib:{0,1,2,4})glDisableVertexAttribArray(attrib);
            VertexArraySetAttribs(&prg);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STREAM_DRAW);
            glUniform1i(prg.frameCountLocation,mFrameCount);glUniform1f(prg.noiseScaleLocation,mCurrentNoiseScale);glUniform1f(prg.prim_depth_location,mCurrentPrimDepth);
            glUniform1iv(prg.texture_width_location,2,widths);glUniform1iv(prg.texture_height_location,2,heights);glUniform1iv(prg.texture_filtering_location,2,filtering);
            glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,reference.data());
            pixels&=actual==reference&&actual[3]==255&&actual[0]>0;++cases;
            glBindVertexArray(cachedVao);glBindBuffer(GL_ARRAY_BUFFER,cachedBuffer);
        }
    }
    unsigned arrays=0;std::vector<GLuint> deleted;bool bothLayouts=true;
    glUseProgram(oldProgram);glBindVertexArray(referenceVao);
    for(auto& prg:probes){
        bothLayouts&=prg.vertexArrays[0]&&prg.vertexArrays[1]&&prg.vertexBuffers[0]==ordinary&&prg.vertexBuffers[1]!=ordinary;
        for(GLuint vao:prg.vertexArrays)if(vao){++arrays;deleted.push_back(vao);}
        glDeleteVertexArrays(2,prg.vertexArrays);prg.vertexArrays[0]=prg.vertexArrays[1]=0;
        glDeleteProgram(prg.openglProgramId);prg.openglProgramId=0;
    }
    bool released=true;for(GLuint vao:deleted)released&=!glIsVertexArray(vao);
    const bool persistent=mPersistentVertices.uploads>uploads;
    const GLenum drawError=glGetError();restore();const GLenum restoreError=glGetError();
    const GLenum error=drawError!=GL_NO_ERROR?drawError:restoreError;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream("native-draw-state.json")<<"{\"session\":\""<<(session?session:"")<<"\",\"cases\":"<<cases<<",\"identical\":"<<identical<<",\"frameOnly\":"<<frameOnly
        <<",\"noiseOnly\":"<<noiseOnly<<",\"drawOnly\":"<<drawOnly<<",\"pixels\":"<<pixels<<",\"uniforms\":"<<uniforms
        <<",\"persistent\":"<<persistent<<",\"requiresPersistent\":"<<requirePersistent<<",\"bothLayouts\":"<<bothLayouts
        <<",\"arrays\":"<<arrays<<",\"released\":"<<released<<",\"glError\":"<<error<<"}";
    if(!pixels||!uniforms||!released||error!=GL_NO_ERROR||cases!=40||
       (requirePersistent&&(!persistent||!bothLayouts||arrays!=4)))throw std::runtime_error("Draw-state cache regression failed");
}
