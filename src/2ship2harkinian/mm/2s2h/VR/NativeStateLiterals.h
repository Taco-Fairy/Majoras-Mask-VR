#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeModuleRanges.h"
#include "save_states/Archive.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <string_view>
namespace mmvrgame {
namespace stateliterals {
inline constexpr std::string_view Prefix="literal/";
// Archive identifiers are limited to 4096 bytes; hex has two characters per byte.
inline constexpr size_t MaxBytes=(4096-Prefix.size())/2;
inline constexpr size_t MaxPoolBytes=16*1024*1024;
struct Pool {
    std::map<std::string,std::string> values;
    size_t bytes=0;
};
// Render-thread state capture/restore only. Nodes and their immutable strings
// are never erased or changed: restored pointers remain valid for process life.
inline Pool& InternPool() {static Pool pool;return pool;}
inline std::optional<std::string> EncodeBounded(const void* pointer,const void* begin,size_t bytes) {
    const auto at=reinterpret_cast<uintptr_t>(pointer),base=reinterpret_cast<uintptr_t>(begin);
    if(!at || !base || bytes>UINTPTR_MAX-base || at<base || at-base>=bytes)return std::nullopt;
    const auto available=std::min(bytes-size_t(at-base),MaxBytes+1);
    const auto* text=static_cast<const unsigned char*>(pointer);
    const auto* end=static_cast<const unsigned char*>(std::memchr(text,0,available));
    if(!end)return std::nullopt;
    std::string result(Prefix);result.reserve(Prefix.size()+size_t(end-text)*2);
    constexpr char hex[]="0123456789abcdef";
    for(auto* it=text;it!=end;++it){result.push_back(hex[*it>>4]);result.push_back(hex[*it&15]);}
    return result;
}
}
// Call ONLY for compiler-described plain-char pointer fields (kind 2), after
// owned-memory resolution. No scanning/inference of untyped pointers is allowed.
// Read-only image membership is supplied by NativeModuleRanges, not inferred
// from the byte contents. Interned strings from an earlier restore are equally
// explicit immutable ownership and permit saving again after a load.
inline std::optional<std::string> EncodeStateLiteral(const void* pointer,
                                                     std::span<const NativeModuleRange> readOnlyImages) {
    for(const auto& range:readOnlyImages)
        if(auto encoded=stateliterals::EncodeBounded(pointer,range.address,range.bytes))return encoded;
    for(const auto& [name,value]:stateliterals::InternPool().values)
        if(auto encoded=stateliterals::EncodeBounded(pointer,value.c_str(),value.size()+1))return encoded;
    return std::nullopt;
}
// The engine deliberately casts __OTR__ resource handles to Gfx*, Vtx*, etc.
// Recognize only this registered engine signature AND an existing mounted asset;
// never reinterpret arbitrary untyped data as a string. No byte is read before
// proving complete prefix membership in an immutable range.
template<class HasAsset>
inline std::optional<std::string> EncodeStateResourceLiteral(const void* pointer,
        std::span<const NativeModuleRange> readOnlyImages,HasAsset&& hasAsset) {
    constexpr std::string_view signature="__OTR__";
    const auto at=reinterpret_cast<uintptr_t>(pointer);
    if(!at)return std::nullopt;
    for(const auto& range:readOnlyImages) {
        const auto base=reinterpret_cast<uintptr_t>(range.address);
        if(!base || range.bytes>UINTPTR_MAX-base || at<base || at-base>=range.bytes ||
           range.bytes-size_t(at-base)<=signature.size())continue;
        if(std::memcmp(pointer,signature.data(),signature.size()))continue;
        auto encoded=stateliterals::EncodeBounded(pointer,range.address,range.bytes);
        if(!encoded)continue;
        const size_t length=(encoded->size()-stateliterals::Prefix.size())/2;
        if(length<=signature.size())continue;
        const std::string_view path(static_cast<const char*>(pointer)+signature.size(),length-signature.size());
        if(hasAsset(path))return encoded;
    }
    return std::nullopt;
}
inline mmvr::states::ExternalRange ResolveStateLiteral(const std::string& name) {
    using namespace stateliterals;
    if(!name.starts_with(Prefix))return {};
    const auto payload=std::string_view(name).substr(Prefix.size());
    if(payload.size()%2 || payload.size()/2>MaxBytes)throw mmvr::states::Error("Invalid saved literal length");
    auto& pool=InternPool();
    if(const auto found=pool.values.find(name);found!=pool.values.end())
        return {const_cast<char*>(found->second.c_str()),found->second.size()+1};
    const auto nibble=[](char c)->int {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        return -1;
    };
    std::string value;value.reserve(payload.size()/2);
    for(size_t i=0;i<payload.size();i+=2) {
        const int a=nibble(payload[i]),b=nibble(payload[i+1]);
        if(a<0||b<0||(a==0&&b==0))throw mmvr::states::Error("Invalid saved literal bytes");
        value.push_back(static_cast<char>((a<<4)|b));
    }
    const auto cost=name.size()+value.size()+1;
    if(cost>MaxPoolBytes-pool.bytes)throw mmvr::states::Error("Saved literal pool limit exceeded");
    auto [it,inserted]=pool.values.emplace(name,std::move(value));
    if(inserted)pool.bytes+=cost;
    return {const_cast<char*>(it->second.c_str()),it->second.size()+1};
}
}
#endif
