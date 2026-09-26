#pragma once
// Isolated native diagnostics: no game archives, settings or save data are used.
#include "ship/resource/ResourceManager.h"
#include "ship/resource/ResourceLoader.h"
#include "ship/resource/ResourceFactory.h"
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
#include <array>
#include "ship/utils/StrHash64.h"
#include "NativeTextChecks.h"
#include "2s2h/resource/type/Cutscene.h"
#include "2s2h/resource/importer/CutsceneFactory.h"
extern "C" {
#include "z64cutscene.h"
}

extern "C" void MMVR_VerifyNativeWidgets();

namespace mmvrtest {
inline void RequireResource(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
class ProbeResource final : public Ship::IResource {
  public:
    char value = 0;
    std::function<void()> onDestroy;
    ProbeResource(std::shared_ptr<Ship::ResourceInitData> data = {}) : IResource(std::move(data)) {}
    ~ProbeResource() override { if (onDestroy) onDestroy(); }
    void* GetRawPointer() override { return &value; }
    size_t GetPointerSize() override { return 1; }
};
inline std::shared_ptr<Ship::ResourceInitData> ProbeMetadata(const std::string& path) {
    auto data = std::make_shared<Ship::ResourceInitData>();
    data->Path=path; data->Format=RESOURCE_FORMAT_BINARY; data->Type=0x56525453;
    data->ResourceVersion=0; data->ByteOrder=Ship::Endianness::Little;
    return data;
}
class ProbeFactory final : public Ship::ResourceFactory {
  public:
    Ship::ResourceManager* manager;
    std::atomic<unsigned> concurrentEntries{0};
    explicit ProbeFactory(Ship::ResourceManager* m) : manager(m) {}
    bool FileHasValidFormatAndReader(std::shared_ptr<Ship::File>, std::shared_ptr<Ship::ResourceInitData>) override { return true; }
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                               std::shared_ptr<Ship::ResourceInitData> data) override {
        if (data->Path == "parent.bin") {
            auto child=manager->LoadResource("__OTR__child.bin",true,ProbeMetadata("child.bin"));
            RequireResource(child!=nullptr,"Nested worker dependency failed");
        }
        if (data->Path == "concurrent.bin") {
            ++concurrentEntries;
            auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(concurrentEntries<8 && std::chrono::steady_clock::now()<until) std::this_thread::yield();
            RequireResource(concurrentEntries==8,"Concurrent factory barrier timed out");
        }
        auto result=std::make_shared<ProbeResource>(data);
        result->value=file->Buffer->at(0);
        return result;
    }
};
inline void VerifyCutsceneMetadata() {
    // Exercise the real binary importer, including the shared NPC-cue parser path.
    for (const auto sample : std::array<std::pair<uint32_t,uint32_t>,3>{{
             {CS_CMD_PLAYER_CUE,1},{CS_CMD_ACTOR_CUE_100,1},{CS_CMD_PLAYER_CUE,0}}}) {
        std::vector<uint32_t> words{32,1,120,sample.first,sample.second};
        for(uint32_t i=0;i<sample.second;++i)
            for(unsigned word=0;word<12;++word) words.push_back(0);
        words.push_back(0xFFFFFFFF);words.push_back(0);
        auto file=std::make_shared<Ship::File>();
        auto reader=std::make_shared<Ship::BinaryReader>(reinterpret_cast<char*>(words.data()),words.size()*sizeof(uint32_t));
        reader->SetEndianness(Ship::Endianness::Native);file->Reader=reader;
        auto data=ProbeMetadata("scene-metadata.bin");
        SOH::ResourceFactoryBinaryCutsceneV0 factory;
        auto resource=std::dynamic_pointer_cast<SOH::Cutscene>(factory.ReadResource(file,data));
        const bool expected=sample.first==CS_CMD_PLAYER_CUE && sample.second>0;
        RequireResource(resource && resource->hasPlayerCue==expected,"Cutscene importer player classification failed");
        RequireResource(SOH::Cutscene::HasPlayerParticipation(resource->GetPointer())==expected,"Imported player metadata publication failed");
    }
    {
        const void* published=nullptr;
        {
            SOH::Cutscene script;script.commands={0,0};script.hasPlayerCue=true;
            published=script.GetPointer();
            RequireResource(SOH::Cutscene::HasPlayerParticipation(published),"Link script metadata not registered");
            RequireResource(!SOH::Cutscene::IsAreaIntroduction(published),"Link cue misclassified as panorama");
            SOH::Cutscene remote;remote.commands={0,0};
            RequireResource(!SOH::Cutscene::HasPlayerParticipation(remote.GetPointer()),"Remote script inherited Link metadata");
        }
        RequireResource(!SOH::Cutscene::HasPlayerParticipation(published),"Destroyed script retained Link metadata");
        SOH::Cutscene replacement;replacement.commands={0,0};
        RequireResource(!SOH::Cutscene::HasPlayerParticipation(replacement.GetPointer()),"Replacement script inherited stale Link metadata");
    }
}
inline void VerifyResourceLifecycle() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic || std::string(diagnostic)!="1") return;
    VerifyNativeText();
    MMVR_VerifyNativeWidgets();
    unsigned checks=0;
    VerifyCutsceneMetadata();++checks;
    {
        Ship::ResourceManager cache;
        cache.CacheExternalResource("marker",std::make_shared<ProbeResource>());
        std::atomic<unsigned> callbacks{0};
        std::atomic<bool> valid{true};
        auto callback=[&] { if (!cache.GetCachedResource("marker",true)) valid=false; ++callbacks; };
        auto first=std::make_shared<ProbeResource>(); first->onDestroy=callback;
        cache.CacheExternalResource("replace",std::move(first));
        cache.CacheExternalResource("replace",std::make_shared<ProbeResource>());
        RequireResource(callbacks==1 && valid,"Replacement destructor did not safely reenter cache");++checks;
        auto second=std::make_shared<ProbeResource>();second->onDestroy=callback;
        cache.CacheExternalResource("remove",std::move(second));
        RequireResource(cache.UnloadResource("remove")==1 && callbacks==2 && valid,"Unload callback/count failed");++checks;
        RequireResource(cache.UnloadResource("remove")==0,"Missing unload returned nonzero");++checks;
        auto plain=std::make_shared<ProbeResource>();cache.CacheExternalResource("item",plain);
        RequireResource(!cache.GetResourceIsCustom(plain),"Metadata-less resource should not be custom");++checks;
        cache.CacheExternalResource("alt/item",std::make_shared<ProbeResource>());
        cache.SetAltAssetsEnabled(true);
        RequireResource(cache.LoadResourceProcess("__OTR__item",true)==plain,"Exact prefixed resource selected alternate");++checks;
        cache.SetAltAssetsEnabled(false);
        std::array<std::thread,4> threads;
        for(unsigned t=0;t<threads.size();++t) threads[t]=std::thread([&,t]{
            for(unsigned i=0;i<1500;++i){
                auto item=std::make_shared<ProbeResource>();
                cache.CacheExternalResource("shared",item);
                if(i%2) item->Dirty();
                cache.GetCachedResource("shared",true);
                cache.SetAltAssetsEnabled((i+t)%2);
                cache.UnloadResource("shared");
            }
        });
        for(auto& thread:threads)thread.join();
        cache.SetAltAssetsEnabled(false);
        RequireResource(cache.GetCachedResource("marker",true)!=nullptr,"Concurrent cache operations lost unrelated resource");++checks;
    }
    const std::filesystem::path fixture="mmvr-resource-fixture";
    RequireResource(!std::filesystem::exists(fixture),"Resource fixture already exists; refusing to overwrite");
    std::filesystem::create_directories(fixture/"a");std::filesystem::create_directories(fixture/"b");
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup(){
            // Exact generated fixture names only; never recurse through caller files.
            for(const char* dir:{"a","b"}) {
                for(const char* name:{"value.bin","parent.bin","child.bin","concurrent.bin","alias.bin.meta","empty.bin","short.bin","broken.xml","no-root.xml","bad.bin.meta","missing.bin.meta","custom.xml","format.bin"})
                    std::filesystem::remove(root/dir/name);
                std::filesystem::remove(root/dir);
            }
            std::filesystem::remove(root);
        }
    } cleanup{fixture};
    for(const char* dir:{"a","b"})for(const char* name:{"value.bin","parent.bin","child.bin","concurrent.bin"}){
        std::array<char,80> bytes{};bytes[0]=dir[0];
        std::ofstream out(fixture/dir/name,std::ios::binary);out.write(bytes.data(),bytes.size());
    }
    auto textFile=[&](const char* name,const char* contents) {
        std::ofstream out(fixture/"a"/name,std::ios::binary);out<<contents;
    };
    textFile("alias.bin.meta",R"({"path":"value.bin","format":"Binary","type":"VRProbe","version":0})");
    textFile("empty.bin","");textFile("short.bin","short");
    textFile("broken.xml","<VRProbe");textFile("no-root.xml","<?xml version=\"1.0\"?>");
    textFile("bad.bin.meta","{ invalid JSON");
    textFile("missing.bin.meta",R"({"path":"does-not-exist.bin","format":"Binary","type":"VRProbe","version":0})");
    textFile("custom.xml","<VRProbe Version=\"0\"/>");textFile("format.bin","format-probe");
    {
        Ship::ResourceManager manager;
        manager.Init({(fixture/"a").string(),(fixture/"b").string()},{},int32_t(std::thread::hardware_concurrency()));
        auto archives=manager.GetArchiveManager();
        RequireResource(!archives->HasFile("absent.bin") && !archives->LoadFile("absent.bin") &&
                        !archives->GetArchiveFromFile("absent.bin") && !archives->HasFile("absent.bin"),
                        "Missing resource lookup mutated archive membership");++checks;
        auto factory=std::make_shared<ProbeFactory>(&manager);
        RequireResource(manager.GetResourceLoader()->RegisterResourceFactory(factory,RESOURCE_FORMAT_BINARY,"VRProbe",0x56525453,0),"Probe factory registration failed");
        RequireResource(manager.GetResourceLoader()->RegisterResourceFactory(factory,RESOURCE_FORMAT_XML,"VRProbe",0x56525453,0),"XML probe registration failed");
        auto alias=manager.LoadResource("alias.bin",true);
        RequireResource(alias && static_cast<ProbeResource*>(alias.get())->value=='b',"Normal metadata alias lost archive priority");++checks;
        auto localAlias=manager.LoadResource({"alias.bin",7,manager.GetArchiveManager()->GetArchives()->at(0)},true);
        RequireResource(localAlias && static_cast<ProbeResource*>(localAlias.get())->value=='a',"Explicit archive metadata escaped into another pack");++checks;
        auto xml=manager.LoadResource("custom.xml",true);
        RequireResource(xml && static_cast<ProbeResource*>(xml.get())->value=='<',"XML did not use its owning loader's factory registry");++checks;
        for(const char* malformed:{"empty.bin","short.bin","broken.xml","no-root.xml","bad.bin","missing.bin"}) {
            RequireResource(!manager.LoadResource(malformed,true),"Malformed resource did not fail safely");++checks;
        }
        auto invalidFormat=ProbeMetadata("format.bin");invalidFormat->Format=0xffffffff;
        RequireResource(!manager.LoadResource("format.bin",true,invalidFormat),"Unknown format was accepted");++checks;
        auto future=manager.LoadResourceAsync("__OTR__parent.bin",true,BS::pr::normal,ProbeMetadata("parent.bin"));
        if(future.wait_for(std::chrono::seconds(5))!=std::future_status::ready){
            std::ofstream("native-resource-lifecycle.json")<<"{\"passed\":false,\"error\":\"single worker dependency deadlock\"}";
            std::_Exit(3); // Diagnostic only: do not hang in the failing pool destructor.
        }
        RequireResource(future.get()!=nullptr,"Prefixed metadata/nested worker load failed");++checks;
        const auto archive=manager.GetArchiveManager()->GetArchives()->at(0);
        auto owned=manager.LoadResourceProcess({"value.bin",7,archive},true,ProbeMetadata("value.bin"));
        auto ordinary=manager.LoadResourceProcess("value.bin",true,ProbeMetadata("value.bin"));
        RequireResource(static_cast<ProbeResource*>(owned.get())->value=='a' && static_cast<ProbeResource*>(ordinary.get())->value=='b',"Explicit source archive was ignored");++checks;
        manager.UnloadResources(Ship::ResourceFilter({"value.bin"},{},7,archive));
        RequireResource(!manager.GetCachedResource({"value.bin",7,archive},true) && manager.GetCachedResource("value.bin",true)==ordinary,"Bulk unload crossed resource ownership");++checks;
        std::array<std::shared_ptr<Ship::IResource>,8> results;
        std::array<std::thread,8> threads;
        for(unsigned i=0;i<threads.size();++i)threads[i]=std::thread([&,i]{results[i]=manager.LoadResourceProcess("concurrent.bin",true,ProbeMetadata("concurrent.bin"));});
        for(auto& thread:threads)thread.join();
        for(const auto& result:results)RequireResource(result && result==results[0],"Concurrent loads published different cached objects");++checks;
        manager.WaitForPendingLoads();
        auto firstArchive=archives->GetArchives()->at(0);
        RequireResource(!firstArchive->LoadFile(CRC64("absent.bin")),"Missing archive hash must be safe");++checks;
        RequireResource(firstArchive->WriteFile("value.bin",std::vector<uint8_t>{'z'}),"Loose pack write failed");
        RequireResource(firstArchive->LoadFile(CRC64("value.bin"))->Buffer->at(0)=='z',"Written loose pack hash did not reload");++checks;
        std::filesystem::remove(fixture/"a"/"value.bin");
        firstArchive->Unload();firstArchive->Load();
        RequireResource(!firstArchive->HasFile("value.bin") && !firstArchive->LoadFile(CRC64("value.bin")),"Deleted loose pack file survived archive reload");++checks;
    }
    std::ofstream("native-resource-lifecycle.json")<<"{\"passed\":true,\"checks\":"<<checks<<",\"concurrentCacheOperations\":6000,\"singleWorkerNestedLoad\":true}";
}
} // namespace mmvrtest
