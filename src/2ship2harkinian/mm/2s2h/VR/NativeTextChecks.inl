template<class Check> void VerifyNativeTextKeyboard(Check check) {
    ContextScope scope;
    ImGui::SetCurrentContext(panel.context);
    const std::string savedSeed=CVarGetString("gRando.InputSeed", "");
    for (int kind=0;kind<3;++kind) {
        char value[64]="";
        CVarSetString("gRando.InputSeed", kind==0 ? "x" : "");
        ImGui::ClearActiveID();
        panel.keyboard=false; panel.pointerMode=true; panel.previousClick=false;
        panel.previousBack=false;panel.queuedKey=panel.heldKey=ImGuiKey_None;
        unsigned ticks=0;
        ImVec2 fieldCenter{512,300};
        auto textStep=[&](ImVec2 cursor, bool click=false) {
            mmvr::NativeMenuInput input{};input.click=click;
            panel.pointer=cursor;
            SyncKeyboard(input);FeedInput(input);ImGui::NewFrame();
            ImGui::SetNextWindowPos({48,190});ImGui::SetNextWindowSize({928,223});
            ImGui::Begin(("Text activation fixture##"+std::to_string(kind)).c_str(),nullptr,ImGuiWindowFlags_NoDecoration);
            if(ticks==0)ImGui::SetWindowFocus();
            if(kind==0) {
                Rando::DrawVrSeedInput();
                if(ticks==0) {
                    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
                    fieldCenter={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};
                    CVarSetString("gRando.InputSeed", "");
                }
            }
            else if(kind==1)ImGui::InputText("Other text",value,sizeof(value));
            else ImGui::InputTextMultiline("Multiline",value,sizeof(value),{500,100});
            if(kind!=0) {
                const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
                fieldCenter={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};
            }
            ImGui::SetItemDefaultFocus();
            ImGui::End();SyncKeyboard(input);DrawKeyboard(input);ImGui::Render();
            std::ofstream("native-keyboard-trace.log",std::ios::app)
                <<kind<<","<<ticks<<","<<panel.keyboard<<","<<panel.context->NavId<<","<<panel.context->ActiveId
                <<","<<panel.context->NavCursorVisible<<","<<panel.context->NavActivateFlags<<","<<panel.keyboardRow<<","<<panel.keyboardColumn<<"\n";
            panel.previousClick=click;panel.previousBack=false;++ticks;
        };
        textStep({512,300});
        textStep(fieldCenter,true);
        check(panel.keyboard,"Dominant trigger did not open keyboard for text field");
        check((kind==0?std::string(CVarGetString("gRando.InputSeed","")):std::string(value)).empty(),"Opening text field typed a stray character");
        textStep({102,800});
        textStep({183,800},true);
        textStep({183,800});
        check((kind==0?std::string(CVarGetString("gRando.InputSeed","")):std::string(value))=="2","Cursor and trigger did not type into field");
        textStep({858,988},true);
        textStep({858,988});
        check(!panel.keyboard && !ImGui::GetCurrentContext()->ActiveId,"Trigger-clicked Enter did not finish text editing");
        check((kind==0?std::string(CVarGetString("gRando.InputSeed","")):std::string(value))=="2","Enter lost or changed typed value");
    }
    CVarSetString("gRando.InputSeed",savedSeed.c_str());
    // A clipped tab still exposes its complete label in a separately sized tooltip.
    panel.keyboard=false;panel.pointerMode=true;ImVec2 tabPoint{0,0};float tabWidth=0;bool tooltip=false;
    for(int tick=0;tick<5;++tick) {
        panel.pointer=tabPoint;FeedInput({});ImGui::NewFrame();
        ImGui::SetNextWindowPos({48,190});ImGui::SetNextWindowSize({180,180});
        ImGui::Begin("Tab tooltip fixture");
        if(ImGui::BeginTabBar("Clipped tabs")) {
            const bool open=BeginNamedTab("Audio Shuffle Pool Management");
            auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
            tabPoint={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};tabWidth=hi.x-lo.x;
            if(open)ImGui::EndTabItem();ImGui::EndTabBar();
        }
        ImGui::End();ImGui::Render();
        if(tick==4)for(auto* window:ImGui::GetCurrentContext()->Windows)
            if(window->Active && (window->Flags&ImGuiWindowFlags_Tooltip) && window->Size.x>tabWidth)tooltip=true;
    }
    check(tooltip,"Clipped tab did not show full-width tooltip");
}
