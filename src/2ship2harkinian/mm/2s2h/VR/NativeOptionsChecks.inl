// Focused private fixture: native pages and the real controller/text-input path.
// It uses the harness's isolated configuration, never the installed player profile.
extern "C" void MMVR_VerifyNativeOptions() {
    if (!mmvr::PrivateDebugTools || !std::getenv("MMVR_NATIVE_OPTIONS_TEST")) return;
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    auto native = std::dynamic_pointer_cast<BenGui::BenMenu>(gui->GetMenu());
    auto* parent = ImGui::GetCurrentContext();
    const auto savedMenu = mmvr::GetMenu();
    auto& menu = mmvr::GetMenu();
    menu.tab=mmvr::NativeTab; menu.open=true; menu.CollapseAll();
    unsigned checks=0;
    auto check=[&](bool ok,const char* label){if(!ok){std::ofstream("native-options-failure.txt")<<label;throw std::runtime_error(label);}++checks;};
    mmvr::UiDrawFrame frame{};frame.kind=mmvr::UiKind::Menu;frame.width=1024;frame.height=mmvr::NativeMenuSurfaceHeight;
    const auto hostFlags = parent->IO.ConfigFlags;
    const auto hostBackendFlags = parent->IO.BackendFlags;
    auto step = [&](float y=0.f, bool a=false, bool back=false, bool collapse=false) {
        auto& input=menu.nativeInput;
        ++input.frame;input.navigateY=y;input.confirm=a;input.back=back;input.collapse=collapse;
        BuildNativeOptions(frame,*gui,true);
    };
    step();step();step();
    check(parent->IO.ConfigFlags==hostFlags && parent->IO.BackendFlags==hostBackendFlags,
          "Native menu changed host navigation flags");
    check((panel.context->IO.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) &&
          (panel.context->IO.BackendFlags & ImGuiBackendFlags_HasGamepad),"Native gamepad navigation not enabled");
    { ContextScope scope;ImGui::SetCurrentContext(panel.context);
      auto* root=ImGui::FindWindowByName("##VR2Ship");
      ImGui::FocusWindow(root);ImGui::SetFocusID(root->GetID("Audio"),root);
    }
    menu.nativeInput.pointerX=.25f; // Opposite-stick drift must not steal navigation.
    const auto firstFocus=panel.context->NavId;
    step(-1);menu.nativeInput.pointerX=0;step();step();
    check(firstFocus && panel.context->NavId && panel.context->NavId!=firstFocus,"Stick did not move actual category focus");
    step(0,true);step();step();
    check(panel.category==1,"A did not enter focused Gameplay category");
    step(0,false,true);step();step();
    check(panel.category==-1 && !menu.nativeCloseRequested,"B did not return from category");
    step(0,false,false,true);step();
    check(!menu.nativeCloseRequested,"Collapse closed root menu");
    step(0,false,true);step();
    check(menu.nativeCloseRequested,"B did not request a safe root close");
    menu.nativeCloseRequested=false;
    std::strcpy(panel.search,"bunny");
    step();step(0,false,true);step();
    check(!panel.search[0] && !menu.nativeCloseRequested,"B must clear search before closing the menu");
    menu.CollapseAll();
    for(int category=-1;category<int(std::size(Categories));++category) {
        for(int tick=0;tick<3;++tick) {
            menu.nativeInput.frame++;
            BuildNativeOptions(frame,*gui,true);
            check(ImGui::GetCurrentContext()==parent,"VR context restoration");
            panel.category=category;
        }
        { ContextScope scope; ImGui::SetCurrentContext(panel.context); check(ImGui::GetDrawData()->TotalVtxCount>0,"Native menu produced no draw data"); }
    }
    {
        ContextScope scope;
        ImGui::SetCurrentContext(panel.context);
        const char* pages[]={"General","Logic/Conditions","Check Pool","Check Exclusions","Item Pool","Starting Items","Hints"};
        for(const char* page:pages) {
            FeedInput({}); ImGui::NewFrame();
            ImGui::SetNextWindowSize({928,480}); ImGui::Begin("Page fixture");
            check(native->DrawVrSection("Rando",page),"Missing randomizer page");
            ImGui::End(); ImGui::Render();
            check(ImGui::GetDrawData()->TotalVtxCount>0,"Empty randomizer draw");
        }
        for(const char* query:{"bunny","blast","bomb mask","no-such-option-zzzz"}){
            FeedInput({});ImGui::NewFrame();
            ImGui::SetNextWindowSize({928,480});ImGui::Begin("Search fixture");
            bool found=native->DrawVrSection("Enhancements","Items/Songs",query);
            check(found==(std::string(query)!="no-such-option-zzzz"),"Mask search result incorrect");
            ImGui::End();ImGui::Render();
        }
        FeedInput({});ImGui::NewFrame();
        ImGui::SetNextWindowSize({928,480});ImGui::Begin("Audio editor fixture");
        DrawVrAudioEditor();ImGui::End();ImGui::Render();
        check(ImGui::GetDrawData()->TotalVtxCount>0,"Empty audio editor draw");
        check(!native->DrawVrSection("Rando","Item Tracker"),"Unrequested tracker exposed");
        check(!native->DrawVrSection("Rando","Check Tracker"),"Unrequested tracker exposed");
        // Real controller mouse injection must toggle a native-style checkbox.
        bool selected=false; ImVec2 target{};
        for(int tick=0;tick<5;++tick) {
            mmvr::NativeMenuInput input{}; input.confirm=tick==2;
            panel.keyboard=false; panel.pointerMode=true; panel.pointer=target;
            FeedInput(input); ImGui::NewFrame();
            ImGui::SetNextWindowPos({48,190});ImGui::SetNextWindowSize({928,480});
            ImGui::Begin("Input fixture",nullptr,ImGuiWindowFlags_NoDecoration);
            ImGui::Checkbox("Controller toggle",&selected);
            target=ImGui::GetItemRectMin();target.x+=8;target.y+=8;
            ImGui::End();ImGui::Render();
        }
        check(selected,"Controller confirmation failed");
        // Keyboard characters must reach the focused field without a mouse click
        // stealing focus; Done must end text editing.
        char text[32]="";
        panel.pointerMode=true; panel.keyboard=true;panel.pointer={102,800};
        for(int tick=0;tick<11;++tick) {
            mmvr::NativeMenuInput input{};input.confirm=tick==2;
            if(tick==4){panel.pointer={183,800};input.confirm=true;}
            if(tick==6){panel.pointer={630,988};input.confirm=true;}
            if(tick==8){panel.pointer={850,988};input.confirm=true;}
            FeedInput(input);ImGui::NewFrame();
            ImGui::SetNextWindowPos({48,190});ImGui::SetNextWindowSize({928,223});
            ImGui::Begin("Keyboard fixture",nullptr,ImGuiWindowFlags_NoDecoration);
            if(tick==0)ImGui::SetKeyboardFocusHere();
            ImGui::InputText("Seed name",text,sizeof(text));
            ImGui::End();DrawKeyboard(input);ImGui::Render();
            panel.previousConfirm=input.confirm;
        }
        check(std::string(text)=="1","On-screen keyboard typing/backspace failed");
        check(panel.context->ActiveId==0,"Keyboard Done did not release text field");
    }
    // Exercise the actual scrolled Randomizer page, not only a small text fixture.
    const int savedSpoiler=CVarGetInteger("gRando.SpoilerFileIndex",0);
    CVarSetInteger("gRando.SpoilerFileIndex",0);
    panel.category=4;panel.pointerMode=false;panel.keyboard=false;
    menu.nativeInput.pointerX=menu.nativeInput.pointerY=0;
    step();step();step();
    {
        ContextScope scope;ImGui::SetCurrentContext(panel.context);
        ImGuiWindow* seedWindow=nullptr;
        for(auto* window:panel.context->Windows)
            if(window->Active && std::string(window->Name).find("randoSettings")!=std::string::npos) seedWindow=window;
        check(seedWindow!=nullptr,"Actual seed editor child missing");
        ImGui::ClearActiveID();ImGui::FocusWindow(seedWindow);
        ImGui::SetFocusID(seedWindow->GetID("##Seed"),seedWindow);
        panel.context->NavCursorVisible=true;
    }
    step(0,true);step();step();
    check(panel.keyboard,"Actual Randomizer page did not retain text focus");
    for(int tick=0;tick<90;++tick)step();
    check(panel.keyboard,"Keyboard disappeared while waiting to type");
    {
        ContextScope scope;ImGui::SetCurrentContext(panel.context);
        auto* editor=ImGui::FindWindowByName("##VR2Ship");
        check(editor && editor->Size.y==480,"Keyboard changed the menu editor height");
        check(KeyboardHit({102,800}) && !KeyboardHit({102,700}),"Keyboard is not below menu");
        check(panel.context->IO.DisplaySize.y==mmvr::NativeMenuSurfaceHeight,"Keyboard surface clipped");
    }
    step(0,false,true);step();step();
    check(!panel.keyboard,"B did not finish real seed editing");
    CVarSetInteger("gRando.SpoilerFileIndex",savedSpoiler);
    VerifyNativeTextKeyboard(check);
    menu.CollapseAll();menu.nativeInput.frame=100;
    BuildNativeOptions(frame,*gui,true);
    check(panel.category==-1&&!panel.keyboard,"Native menu did not reset on tab change");
    menu=savedMenu;
    check(ImGui::GetCurrentContext()==parent,"Final context changed");
    std::ofstream("native-options-checks.json")<<"{\"passed\":true,\"checks\":"<<checks<<"}";
}
