// Scripted sequences and interaction sequences occupy separate pages. Keep
// every page's geometry alive because an interpolated frame may still replay
// the previous page's display list.
int cutsceneGroup = 0, cutscenePage = 0, cutsceneLastPad = -1;
constexpr int cutscenePerPage = 4;
constexpr int interactiveCutsceneCount = ARRAY_COUNT(debugCutscenes) - scriptedCutsceneCount + visibleNativeTriggerCount;
constexpr int scriptedCutscenePages = (scriptedCutsceneCount + cutscenePerPage - 1) / cutscenePerPage;
constexpr int interactiveCutscenePages = (interactiveCutsceneCount + cutscenePerPage - 1) / cutscenePerPage;
std::array<Mesh, scriptedCutscenePages + interactiveCutscenePages> cutsceneMeshes;
int CutscenePageCount() { return cutsceneGroup ? interactiveCutscenePages : scriptedCutscenePages; }
int CutsceneEntryIndex(int pad) { return (cutsceneGroup ? scriptedCutsceneCount : 0) + cutscenePage * cutscenePerPage + pad; }
Mesh& CutsceneCatalogMesh() { return cutsceneMeshes[(cutsceneGroup ? scriptedCutscenePages : 0) + cutscenePage]; }
void BuildCutsceneCatalogPage() {
    auto& mesh = CutsceneCatalogMesh();
    if (!mesh.commands.empty()) return;
    for (int i = 0; i < cutscenePerPage; ++i) {
        int index = CutsceneEntryIndex(i);
        if (index >= ARRAY_COUNT(debugCutscenes) + visibleNativeTriggerCount ||
            (index < ARRAY_COUNT(debugCutscenes) && debugCutscenes[index].interactive != bool(cutsceneGroup))) break;
        short x = short(2900 + i * 440), z = -1700;
        mesh.Quad({short(x - 44), 2, short(z - 44)}, {short(x - 44), 2, short(z + 44)},
                  {short(x + 44), 2, short(z + 44)}, {short(x + 44), 2, short(z - 44)}, purple);
        std::string label = index < ARRAY_COUNT(debugCutscenes)
                                ? debugCutscenes[index].name
                                : debugNativeTriggers[index - ARRAY_COUNT(debugCutscenes)].name;
        for (size_t p = 0; p < label.size(); ++p) {
            if (label[p] >= 'a' && label[p] <= 'z') label[p] -= 32;
            if (label[p] >= '0' && label[p] <= '9') {
                constexpr const char* digits[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};
                const std::string word = digits[label[p] - '0'];
                label.replace(p, 1, word); p += word.size() - 1;
            }
            if (label[p] == '\'' || label[p] == '-') label[p] = ' ';
        }
        Sign(label.c_str(), x, 100, short(z - 70), 1, &mesh);
    }
    std::string page = "PAGE "; page += char('A' + cutscenePage);
    Sign(page.c_str(), 3600, 105, -2530, 2, &mesh);
    Sign(cutsceneGroup ? "INTERACTIVE CUTSCENES" : "SCRIPTED CUTSCENES", 3600, 170, -3180, 2, &mesh);
    for (short x : {short(3150), short(4050)}) {
        mesh.Quad({short(x - 44), 2, -2644}, {short(x - 44), 2, -2556},
                  {short(x + 44), 2, -2556}, {short(x + 44), 2, -2644}, green);
    }
    mesh.Finish();
}
void UpdateCutsceneCatalogPads(PlayState* play, Player* player) {
    if (play->msgCtx.msgMode != MSGMODE_NONE || (player->stateFlags1 & PLAYER_STATE1_TALKING)) return;
    auto pos = player->actor.world.pos;
    int pad = -1;
    if (std::abs(pos.y) < 12) {
        for (int i = 0; i < cutscenePerPage; ++i)
            if (std::abs(pos.x - (2900 + i * 440)) < 44 && std::abs(pos.z + 1700) < 44) pad = i;
        if (std::abs(pos.x - 3150) < 44 && std::abs(pos.z + 2600) < 44) pad = 4;
        if (std::abs(pos.x - 4050) < 44 && std::abs(pos.z + 2600) < 44) pad = 5;
        if (std::abs(pos.x - 3150) < 44 && std::abs(pos.z + 3000) < 44) pad = 6;
        if (std::abs(pos.x - 4050) < 44 && std::abs(pos.z + 3000) < 44) pad = 7;
    }
    if (pad >= 0 && pad != cutsceneLastPad) {
        if (pad == 4 || pad == 5) {
            cutscenePage = (cutscenePage + (pad == 5 ? 1 : CutscenePageCount() - 1)) % CutscenePageCount();
            BuildCutsceneCatalogPage(); Audio_PlaySfx(NA_SE_SY_DECIDE);
        } else if (pad == 6 || pad == 7) {
            cutsceneGroup = pad == 7; cutscenePage = 0;
            BuildCutsceneCatalogPage(); Audio_PlaySfx(NA_SE_SY_DECIDE);
        } else {
            const int index = CutsceneEntryIndex(pad);
            if (index < ARRAY_COUNT(debugCutscenes) && debugCutscenes[index].interactive == bool(cutsceneGroup))
                MMVR_DebugCutsceneBegin(play, index);
            else if (cutsceneGroup && index < ARRAY_COUNT(debugCutscenes) + visibleNativeTriggerCount)
                MMVR_DebugNativeTriggerBegin(play, index - ARRAY_COUNT(debugCutscenes));
        }
    }
    cutsceneLastPad = pad;
}
