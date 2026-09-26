template<class Check> void VerifyNativeTextKeyboard(Check check) {
    ContextScope scope;
    ImGui::SetCurrentContext(panel.context);
    const std::string savedSeed=CVarGetString("gRando.InputSeed", "");
    for (int kind=0;kind<3;++kind) {
        char value[64]="";
        CVarSetString("gRando.InputSeed", "");
        ImGui::ClearActiveID();
        panel.keyboard=false; panel.pointerMode=false; panel.previousConfirm=false;
        panel.previousBack=false;panel.queuedKey=panel.heldKey=ImGuiKey_None;
        unsigned ticks=0;
        auto textStep=[&](float x=0,float y=0,bool confirm=false) {
            mmvr::NativeMenuInput input{};input.navigateX=x;input.navigateY=y;input.confirm=confirm;
            SyncKeyboard(input);FeedInput(input);ImGui::NewFrame();
            ImGui::SetNextWindowPos({48,190});ImGui::SetNextWindowSize({928,223});
            ImGui::Begin(("Text activation fixture##"+std::to_string(kind)).c_str(),nullptr,ImGuiWindowFlags_NoDecoration);
            if(ticks==0)ImGui::SetWindowFocus();
            if(kind==0)Rando::DrawVrSeedInput();
            else if(kind==1)ImGui::InputText("Other text",value,sizeof(value));
            else ImGui::InputTextMultiline("Multiline",value,sizeof(value),{500,100});
            ImGui::SetItemDefaultFocus();
            ImGui::End();SyncKeyboard(input);DrawKeyboard(input);ImGui::Render();
            std::ofstream("native-keyboard-trace.log",std::ios::app)
                <<kind<<","<<ticks<<","<<panel.keyboard<<","<<panel.context->NavId<<","<<panel.context->ActiveId
                <<","<<panel.context->NavCursorVisible<<","<<panel.context->NavActivateFlags<<","<<panel.keyboardRow<<","<<panel.keyboardColumn<<"\n";
            panel.previousConfirm=confirm;panel.previousBack=false;++ticks;
        };
        for(int i=0;i<5;++i)textStep();
        textStep(0,-1);textStep();textStep(); // Resume stick navigation after the pointer fixture.
        textStep(0,0,true);textStep();textStep();
        check(panel.keyboard,"A did not open keyboard for text field");
        check((kind==0?std::string(CVarGetString("gRando.InputSeed","")):std::string(value)).empty(),"Opening A typed a stray character");
        textStep(1,0);textStep();textStep(0,0,true);textStep();textStep();
        check((kind==0?std::string(CVarGetString("gRando.InputSeed","")):std::string(value))=="2","Keyboard navigation did not type into field");
        for(int i=0;i<4;++i){textStep(0,-1);textStep();}
        for(int i=0;i<2;++i){textStep(1,0);textStep();}
        textStep(0,0,true);textStep();textStep();
        check(!panel.keyboard && !ImGui::GetCurrentContext()->ActiveId,"Enter did not finish text editing");
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
