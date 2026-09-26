#pragma once

// Private native diagnostic for the synchronous, owned cache-hit path. It
// needs no archives, game state, renderer, or headset.
#include "ship/resource/ResourceManager.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace Ship {
class ResourceLookupProbe final : public IResource {
  public:
    ResourceLookupProbe() : IResource(nullptr) {}
    void* GetRawPointer() override { return &value; }
    size_t GetPointerSize() override { return sizeof(value); }
    char value = 0;
};

inline void VerifyResourceLookupChecks() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic || std::string_view(diagnostic)!="1")
        throw std::runtime_error("Transparent resource lookup requires native-test mode");
    ResourceManager cache;
    const std::string path="__mmvr_transparent_resource_lookup_probe__";
    const std::string altPath="alt/"+path;
    const std::string prefixedPath="__OTR__"+path;
    const char* borrowed=path.c_str();
    auto base=std::make_shared<ResourceLookupProbe>();
    auto alt=std::make_shared<ResourceLookupProbe>();
    auto replacement=std::make_shared<ResourceLookupProbe>();
    cache.CacheExternalResource(path,base);
    cache.CacheExternalResource(altPath,alt);
    bool correct=cache.LoadResource(path)==base && cache.LoadResourceProcess(path)==base &&
                 cache.LoadResourceProcess(borrowed)==base &&
                 cache.LoadResourceProcess(static_cast<const char*>(nullptr))==nullptr &&
                 cache.LoadResource("__OTR__"+path,true)==base &&
                 cache.LoadResourceProcess(prefixedPath,true)==base &&
                 cache.LoadResourceProcess(prefixedPath.c_str(),true)==base;
    cache.SetAltAssetsEnabled(true);
    const bool altCorrect=cache.LoadResource(path)==alt && cache.LoadResourceProcess(path)==alt &&
                          cache.LoadResourceProcess(borrowed)==alt &&
                          cache.LoadResource(path,true)==base && cache.LoadResourceProcess(path,true)==base &&
                          cache.LoadResourceProcess(borrowed,true)==base;
    cache.SetAltAssetsEnabled(false);
    correct &= altCorrect && cache.LoadResource(path)==base && cache.LoadResourceProcess(path)==base;
    cache.CacheExternalResource(path,replacement);
    const bool replaced=cache.LoadResource(path)==replacement && cache.LoadResourceProcess(path)==replacement &&
                        cache.LoadResourceProcess(borrowed)==replacement;
    replacement->Dirty();
    const bool dirtyRejected=!cache.GetCachedResource(path);
    cache.CacheExternalResource(path,base);
    correct &= replaced && dirtyRejected && cache.LoadResource(path)==base &&
               cache.LoadResourceProcess(path)==base && cache.LoadResourceProcess(borrowed)==base;

    constexpr int iterations=10000;
    std::array<double,6> identifierNs{},pathNs{},processIdentifierNs{},processPathNs{},
                         processTemporaryNs{},processCharNs{};
    bool identity=true;
    for(int run=0;run<6;++run)for(int slot=0;slot<4;++slot){
        const bool process=slot>=2;
        const bool viaPath=(run+slot)%2;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<iterations;++i){
            auto resource=process ?
                (viaPath ? cache.LoadResourceProcess(path) :
                           cache.LoadResourceProcess(ResourceIdentifier{path,0,nullptr})) :
                (viaPath ? cache.LoadResource(path) :
                           cache.LoadResource(ResourceIdentifier{path,0,nullptr}));
            identity &= resource==base;
        }
        const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/iterations;
        (process ? (viaPath ? processPathNs : processIdentifierNs) :
                   (viaPath ? pathNs : identifierNs))[run]=ns;
    }
    for(auto* samples:{&identifierNs,&pathNs,&processIdentifierNs,&processPathNs})
        std::sort(samples->begin(),samples->end());
    for(int run=0;run<6;++run)for(int pass=0;pass<2;++pass){
        const bool direct=(run+pass)%2;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<iterations;++i){
            auto resource=direct ? cache.LoadResourceProcess(borrowed) :
                                   cache.LoadResourceProcess(std::string(borrowed));
            identity &= resource==base;
        }
        const double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/iterations;
        (direct?processCharNs:processTemporaryNs)[run]=ns;
    }
    std::sort(processTemporaryNs.begin(),processTemporaryNs.end());
    std::sort(processCharNs.begin(),processCharNs.end());
    const bool unloaded=cache.UnloadResource(path)==1 && !cache.GetCachedResource(path) &&
                        cache.UnloadResource(altPath)==1;
    correct &= identity && unloaded;
    std::ofstream output("native-resource-transparent.json");
    output<<"{\"correct\":"<<correct<<",\"alt\":"<<altCorrect<<",\"replacement\":"<<replaced
          <<",\"dirtyRejected\":"<<dirtyRejected<<",\"unloaded\":"<<unloaded
          <<",\"iterationsPerSample\":"<<iterations<<",\"samples\":6"
          <<",\"identifierMedianNs\":"<<(identifierNs[2]+identifierNs[3])/2
          <<",\"transparentMedianNs\":"<<(pathNs[2]+pathNs[3])/2
          <<",\"processIdentifierMedianNs\":"<<(processIdentifierNs[2]+processIdentifierNs[3])/2
          <<",\"processTransparentMedianNs\":"<<(processPathNs[2]+processPathNs[3])/2
          <<",\"processTemporaryStringMedianNs\":"<<(processTemporaryNs[2]+processTemporaryNs[3])/2
          <<",\"processCharMedianNs\":"<<(processCharNs[2]+processCharNs[3])/2<<"}";
    if(!correct)throw std::runtime_error("Transparent resource lookup regression");
}
} // namespace Ship
