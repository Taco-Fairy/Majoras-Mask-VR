#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeModuleRanges.h"
#include "NativeStateCompatibility.h"
#include "save_states/Fingerprint.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/Resource.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/Archive.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#ifndef _WIN32
#include <dlfcn.h>
#endif
extern "C" void MMVR_PrepareDebugStateAssets();
namespace mmvrgame {
void RegisterManualNativeStateContract(MMVR_StateSink* sink);
inline std::filesystem::path NativeStateModulePath() {
#ifdef _WIN32
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&NativeModuleRanges),&module))throw mmvr::states::Error("Cannot identify game module");
    std::wstring path(32768,L'\0');auto count=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(!count||count>=path.size())throw mmvr::states::Error("Cannot identify game module path");
    path.resize(count);return path;
#else
    Dl_info info{};
    if(!dladdr(reinterpret_cast<const void*>(&NativeModuleRanges),&info)||!info.dli_fname)
        throw mmvr::states::Error("Cannot identify game module path");
    return info.dli_fname;
#endif
}
inline mmvr::states::Identity NativeStateIdentity() {
    using namespace mmvr::states;
    Fingerprint assets;
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    auto archives=manager->GetArchives();
    if(!archives||archives->empty())throw Error("No mounted state content");
    // Priority order is part of identity. Changing mod order changes resolution.
    for(const auto& archive:*archives) {
        if(!archive)throw Error("Missing mounted state archive");
        const std::filesystem::path path=archive->GetPath();
        assets.Field(path.generic_string());
        if(std::filesystem::is_regular_file(path))assets.Field(FingerprintFile(path));
        else {
            // Use the archive's existing index, not an unrestricted filesystem scan.
            std::vector<std::string> names;
            for(const auto& [hash,name]:*archive->ListFiles())names.push_back(name);
            std::sort(names.begin(),names.end());
            for(const auto& name:names) {
                auto file=archive->LoadFile(name);
                if(!file||!file->IsLoaded||!file->Buffer)throw Error("Unreadable mounted state asset: "+name);
                Fingerprint bytes;bytes.Add({reinterpret_cast<const uint8_t*>(file->Buffer->data()),file->Buffer->size()});
                assets.Field(name);assets.Field(bytes.Hex());
            }
        }
    }
#ifdef _WIN32
    const char* platform="windows-x64";
#elif defined(__ANDROID__) && defined(__aarch64__)
    const char* platform="android-arm64";
#else
    const char* platform="unsupported";
#endif
    if(std::endian::native!=std::endian::little||sizeof(uintptr_t)!=8||std::string_view(platform)=="unsupported")
        throw Error("Unsupported native save-state ABI");
    // Cross-update restoration is not validated. Fail closed on a different binary.
    static const std::string moduleDigest=FingerprintFile(NativeStateModulePath());
    return {"native-development-v3/"+moduleDigest,assets.Hex(),std::string(platform)+"/64-le"};
}
inline constexpr const char* StateResourcesId="engine/resource-manifest";
inline nlohmann::json ResourceEntry(const Ship::ResourceManager::CachedResourceView& entry) {
    using mmvr::states::Error;
    if(entry.identifier.Owner)throw Error("State resource has an unsupported process-local owner: "+entry.identifier.Path);
    if(!entry.resource||entry.resource->IsDirty())throw Error("State resource is unavailable or dirty: "+entry.identifier.Path);
    auto info=entry.resource->GetInitData();if(!info)throw Error("State resource lacks initialization metadata");
    return {{"path",entry.identifier.Path},{"parent",entry.identifier.Parent?entry.identifier.Parent->GetPath():""},
            {"type",info->Type},{"version",info->ResourceVersion},{"format",info->Format},{"id",info->Id},{"custom",info->IsCustom}};
}
inline std::vector<Ship::ResourceIdentifier> ReadStateResourceManifest(const mmvr::states::Block& block) {
    using namespace mmvr::states;
    if(block.id!=StateResourcesId||block.schema!=1||!block.references.empty()||block.bytes.size()>16*1024*1024)
        throw Error("Invalid state resource manifest");
    auto entries=nlohmann::json::parse(block.bytes);
    if(!entries.is_array()||entries.size()>65536)throw Error("Invalid state resource count");
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    auto archives=manager->GetArchiveManager()->GetArchives();
    std::vector<Ship::ResourceIdentifier> result;std::set<std::pair<std::string,std::string>> seen;
    for(const auto& entry:entries) {
        auto path=entry.at("path").get<std::string>(),parent=entry.at("parent").get<std::string>();
        if(path.empty()||path.size()>4096||parent.size()>32768||path.find('\0')!=std::string::npos||parent.find('\0')!=std::string::npos||
           !seen.emplace(parent,path).second)throw Error("Invalid/duplicate state resource path");
        std::shared_ptr<Ship::Archive> origin;
        if(!parent.empty()) {
            for(const auto& archive:*archives)if(archive->GetPath()==parent) {
                if(origin)throw Error("Ambiguous state archive");origin=archive;
            }
            if(!origin)throw Error("State archive is no longer mounted");
        }
        // This does not mount files or follow arbitrary paths from the state.
        if(origin?!origin->HasFile(path):!manager->GetArchiveManager()->HasFile(path))
            throw Error("State resource is not present in mounted content: "+path);
        result.emplace_back(std::move(path),0,std::move(origin));
    }
    return result;
}
inline void LoadStateResourceManifest(const mmvr::states::Snapshot& saved) {
    using namespace mmvr::states;
    const auto found=std::find_if(saved.blocks.begin(),saved.blocks.end(),[](const Block& block){return block.id==StateResourcesId;});
    if(found==saved.blocks.end())throw Error("Missing state resource manifest");
    // A new process need not have visited the procedural room. Construct its
    // immutable geometry before binding saved references, without entering it.
    bool needsDebugAssets=false;
    for(const auto& block:saved.blocks)for(const auto& reference:block.references)
        if(reference.kind==ReferenceKind::Asset&&reference.target.starts_with("debug/"))needsDebugAssets=true;
    if(needsDebugAssets)MMVR_PrepareDebugStateAssets();
    auto identifiers=ReadStateResourceManifest(*found);
    const auto expected=nlohmann::json::parse(found->bytes);
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    manager->WaitForPendingLoads();
    for(size_t i=0;i<identifiers.size();++i) {
        auto resource=manager->LoadResource(identifiers[i],true);
        if(!resource||ResourceEntry({identifiers[i],resource})!=expected[i])
            throw Error("State resource metadata changed: "+identifiers[i].Path);
    }
    manager->WaitForPendingLoads();
}
inline mmvr::states::Component StateResourceManifestComponent() {
    using namespace mmvr::states;
    struct Prepared final:PreparedComponent {void Commit() noexcept override {}};
    return {StateResourcesId,1,[] {
        auto entries=nlohmann::json::array();
        auto resources=Ship::Context::GetRawInstance()->GetResourceManager()->SnapshotCachedResources();
        for(const auto& entry:resources)entries.push_back(ResourceEntry(entry));
        std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){
            return std::pair{a.at("parent").template get<std::string>(),a.at("path").template get<std::string>()}<
                   std::pair{b.at("parent").template get<std::string>(),b.at("path").template get<std::string>()};});
        auto text=entries.dump();Block block{StateResourcesId,1};block.bytes.assign(text.begin(),text.end());
        ReadStateResourceManifest(block);return block;
    },[](const Block& block)->std::unique_ptr<PreparedComponent> {
        auto identifiers=ReadStateResourceManifest(block);auto expected=nlohmann::json::parse(block.bytes);
        auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
        for(size_t i=0;i<identifiers.size();++i) {
            auto resource=manager->GetCachedResource(identifiers[i],true);
            if(!resource||ResourceEntry({identifiers[i],resource})!=expected[i])throw Error("State asset was not prepared");
        }
        return std::make_unique<Prepared>();
    }};
}
// Resolves private callbacks even if that actor/action is absent in the current
// scene. Caller MUST first validate the exact on-disk executable fingerprint.
inline mmvr::states::ExternalRange ResolveStateImageFunction(const std::string& name) {
    for(const auto& section:NativeModuleRanges())if(section.executable) {
        const auto prefix=section.id+"/function/";
        if(!name.starts_with(prefix))continue;
        uint64_t offset=0;const auto* first=name.data()+prefix.size();const auto* end=name.data()+name.size();
        auto parsed=std::from_chars(first,end,offset);
        if(parsed.ec!=std::errc{}||parsed.ptr!=end||offset>=section.bytes)return {};
        return {const_cast<uint8_t*>(static_cast<const uint8_t*>(section.address))+offset,0};
    }
    return {};
}
}
#endif
