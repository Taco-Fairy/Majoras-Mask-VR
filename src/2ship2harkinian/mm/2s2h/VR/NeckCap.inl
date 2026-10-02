// Close the five-edge opening in the native human torso, in that limb's
// coordinates. It shares the existing skin texture/palette and lighting.
// Never replaces a head or changes a skeleton/camera transform.
static Vtx humanNeckCap[] = {
    {{{986,-256,-118},0,{-85,-50},{116,237,236,255}}},
    {{{986,-256, 118},0,{-85,-50},{113,220,13,255}}},
    {{{921, -91, 187},0,{-1,40},{102,39,48,255}}},
    {{{831,  97,   0},0,{48,-7},{100,66,0,255}}},
    {{{921, -91,-187},0,{-1,40},{102,39,208,255}}},
};
static Gfx humanNeckCapDL[] = {
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(gLinkHumanSkinTLUT),
    gsDPLoadTextureBlock(object_link_child_Tex_005500,G_IM_FMT_CI,G_IM_SIZ_8b,8,8,0,
                        G_TX_WRAP,G_TX_WRAP,3,3,G_TX_NOLOD,G_TX_NOLOD),
    gsDPSetPrimColor(0,0,255,255,255,255),
    gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2),
    gsSPClearGeometryMode(G_CULL_FRONT|G_CULL_BACK),
    gsSPVertex(humanNeckCap,5,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0),
    gsSP1Triangle(0,3,4,0),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};
extern "C" const void* MMVR_PlayerNeckCap(Actor* actor,int limb) {
    if(!gPlayState || actor!=(Actor*)GET_PLAYER(gPlayState) || limb!=PLAYER_LIMB_TORSO ||
       ((Player*)actor)->transformation!=PLAYER_FORM_HUMAN || MMVR_ControlledKafei((Player*)actor) ||
       mmvr::GetSettings().Get(mmvr::Setting::FullBody)<.5f || !HideCurrentPlayer(gPlayState)) return nullptr;
    return humanNeckCapDL;
}
