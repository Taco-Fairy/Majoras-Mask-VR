#include "notebook_book.h"
#include "assets/objects/object_gi_schedule/object_gi_schedule.h"

// Solid millimetre geometry under the two compositor leaves. Native notebook
// textures remain resource-managed, including texture-pack replacements.
extern "C" void MMVR_DrawNotebookBinding(PlayState* play) {
    mmvr::SetNotebookModelMatrix(nullptr);
    if(!play || !MMVR_NotebookBook()) return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,play->view.projectionPtr,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_PROJECTION);
    gSPMatrix(POLY_OPA_DISP++,play->view.viewingPtr,G_MTX_NOPUSH|G_MTX_MUL|G_MTX_PROJECTION);
    auto* matrix=(Mtx*)GRAPH_ALLOC(play->state.gfxCtx,sizeof(Mtx));
    std::memset(matrix,0,sizeof(Mtx)); // Flat/theater fallback does not draw a floating binding.
    mmvr::SetNotebookModelMatrix(matrix);
    gSPMatrix(POLY_OPA_DISP++,matrix,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
    gSPClearGeometryMode(POLY_OPA_DISP++,G_LIGHTING|G_CULL_BACK|G_CULL_FRONT|G_FOG);
    gSPClearExtraGeometryMode(POLY_OPA_DISP++,G_EX_INVERT_CULLING);
    gDPSetTextureLUT(POLY_OPA_DISP++,G_TT_NONE);
    gDPSetCycleType(POLY_OPA_DISP++,G_CYC_1CYCLE);
    // SetupDL25 carries a fog-alpha blender even after G_FOG is cleared.
    // This palm prop must not inherit the scene's fog color through vertex alpha.
    gDPSetRenderMode(POLY_OPA_DISP++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineLERP(POLY_OPA_DISP++,PRIMITIVE,ENVIRONMENT,TEXEL0,ENVIRONMENT,0,0,0,1,
                     PRIMITIVE,ENVIRONMENT,TEXEL0,ENVIRONMENT,0,0,0,1);
    gSPTexture(POLY_OPA_DISP++,0xFFFF,0xFFFF,0,G_TX_RENDERTILE,G_ON);
    // Four vertices at a time keep the native vertex cache bounded.
    auto face=[&](int leaf,float x0,float y0,float z0,float x1,float y1,float z1,
                  float x2,float y2,float z2,float x3,float y3,float z3,int tw,int th) {
        auto* v=(Vtx*)GRAPH_ALLOC(play->state.gfxCtx,4*sizeof(Vtx));
        const float xyz[4][3]={{x0,y0,z0},{x1,y1,z1},{x2,y2,z2},{x3,y3,z3}};
        const auto pose=leaf<0 ? mmvr::YawPose(0) : mmvr::NotebookLeafLocal(leaf);
        for(int i=0;i<4;++i) {
            std::memset(&v[i],0,sizeof(Vtx));
            for(int c=0;c<3;++c) {
                float n=pose.m[3][c]*1000;
                for(int k=0;k<3;++k)n+=xyz[i][k]*pose.m[k][c];
                v[i].v.ob[c]=(s16)std::lround(n);
            }
            v[i].v.tc[0]=(i==1||i==2)?tw*32:0;
            v[i].v.tc[1]=(i>=2)?th*32:0;
            for(int k=0;k<4;++k)v[i].v.cn[k]=255;
        }
        gSPVertex(POLY_OPA_DISP++,uintptr_t(v),4,0);
        gSP2Triangles(POLY_OPA_DISP++,0,1,2,0,0,2,3,0);
    };
    const float w=mmvr::NotebookWidth*250.f,h=mmvr::NotebookHeight*500.f;
    for(int leaf=0;leaf<2;++leaf) {
        // Purple native hard cover, a small overhang and a 6mm paper block.
        gDPSetPrimColor(POLY_OPA_DISP++,0,0,185,185,255,255);
        gDPSetEnvColor(POLY_OPA_DISP++,55,0,55,255);
        gDPLoadTextureBlock(POLY_OPA_DISP++,gGiBombersNotebookCoverTex,G_IM_FMT_I,G_IM_SIZ_8b,32,32,0,
                           G_TX_WRAP,G_TX_WRAP,5,5,G_TX_NOLOD,G_TX_NOLOD);
        const float a=w+5,b=h+5;
        face(leaf,-a,-b,-8,a,-b,-8,a,b,-8,-a,b,-8,32,32);
        face(leaf,-a,b,-10,a,b,-10,a,-b,-10,-a,-b,-10,32,32);
        face(leaf,-a,-b,-10,a,-b,-10,a,-b,-8,-a,-b,-8,32,2);
        face(leaf,a,b,-10,-a,b,-10,-a,b,-8,a,b,-8,32,2);
        face(leaf,-a,b,-10,-a,-b,-10,-a,-b,-8,-a,b,-8,32,2);
        face(leaf,a,-b,-10,a,b,-10,a,b,-8,a,-b,-8,32,2);
        gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,255,215,255);
        gDPSetEnvColor(POLY_OPA_DISP++,150,150,120,255);
        gDPLoadTextureBlock_4b(POLY_OPA_DISP++,gGiBombersNotebookPagesTex,G_IM_FMT_I,16,8,0,
                              G_TX_WRAP,G_TX_WRAP,4,3,G_TX_NOLOD,G_TX_NOLOD);
        face(leaf,-w,-h,-7,w,-h,-7,w,-h,-1,-w,-h,-1,16,8);
        face(leaf,w,h,-7,-w,h,-7,-w,h,-1,w,h,-1,16,8);
        face(leaf,-w,h,-7,-w,-h,-7,-w,-h,-1,-w,h,-1,16,8);
        face(leaf,w,-h,-7,w,h,-7,w,h,-1,w,-h,-1,16,8);
        // Keep the underside recognizable as the inventory book, with its native label.
        if(leaf==1) {
            gDPSetPrimColor(POLY_OPA_DISP++,0,0,255,100,0,255);
            gDPSetEnvColor(POLY_OPA_DISP++,255,255,195,255);
            gDPLoadTextureBlock_4b(POLY_OPA_DISP++,gGiBombersNotebookLabelTex,G_IM_FMT_I,32,64,0,
                                  G_TX_CLAMP,G_TX_CLAMP,5,6,G_TX_NOLOD,G_TX_NOLOD);
            face(leaf,w*.7f,h*.75f,-11,-w*.7f,h*.75f,-11,-w*.7f,-h*.75f,-11,w*.7f,-h*.75f,-11,32,64);
        }
    }
    // A rounded-looking narrow spine and three native binding bands bridge the hinge.
    gDPSetPrimColor(POLY_OPA_DISP++,0,0,185,185,255,255);
    gDPSetEnvColor(POLY_OPA_DISP++,55,0,55,255);
    gDPLoadTextureBlock_4b(POLY_OPA_DISP++,gGiBombersNotebookBindingsTex,G_IM_FMT_I,16,32,0,
                          G_TX_WRAP,G_TX_WRAP,4,5,G_TX_NOLOD,G_TX_NOLOD);
    face(-1,-6,-h-5,-11,6,-h-5,-11,6,h+5,-11,-6,h+5,-11,16,32);
    for(float y:{-h*.7f,0.f,h*.7f}) {
        face(-1,-9,y-4,-10,-9,y+4,-10,0,y+4,-3,0,y-4,-3,16,8);
        face(-1,0,y-4,-3,0,y+4,-3,9,y+4,-10,9,y-4,-10,16,8);
    }
    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
}

