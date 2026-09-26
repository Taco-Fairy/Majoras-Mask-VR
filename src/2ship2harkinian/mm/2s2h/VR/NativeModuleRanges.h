#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include <cstdint>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <link.h>
#endif
namespace mmvrgame {
struct NativeModuleRange {std::string id;const void* address;size_t bytes;bool executable;};
// Exact-build states can identify literals/private callback functions by image
// section and offset. Writable sections are deliberately excluded: they contain
// C++/OS objects and must never be copied as an undifferentiated memory dump.
inline std::vector<NativeModuleRange> NativeModuleRanges() {
    std::vector<NativeModuleRange> result;
    const auto anchor=reinterpret_cast<uintptr_t>(&NativeModuleRanges);
#ifdef _WIN32
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(anchor),&module))return result;
    auto base=reinterpret_cast<const uint8_t*>(module);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||nt->Signature!=IMAGE_NT_SIGNATURE)return result;
    auto section=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        auto flags=section[i].Characteristics;
        if(!(flags&IMAGE_SCN_MEM_READ)||(flags&(IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_DISCARDABLE)))continue;
        auto size=section[i].Misc.VirtualSize;
        if(size)result.push_back({"game-image/"+std::to_string(i),base+section[i].VirtualAddress,size,
                                 bool(flags&IMAGE_SCN_MEM_EXECUTE)});
    }
#else
    struct Context {uintptr_t anchor;std::vector<NativeModuleRange>* ranges;} context{anchor,&result};
    dl_iterate_phdr([](dl_phdr_info* info,size_t,void* opaque) {
        auto& context=*static_cast<Context*>(opaque);bool contains=false;
        for(unsigned i=0;i<info->dlpi_phnum;++i) {
            const auto& h=info->dlpi_phdr[i];auto start=info->dlpi_addr+h.p_vaddr;
            if(h.p_type==PT_LOAD&&context.anchor>=start&&context.anchor-start<h.p_memsz)contains=true;
        }
        if(!contains)return 0;
        for(unsigned i=0;i<info->dlpi_phnum;++i) {
            const auto& h=info->dlpi_phdr[i];
            // RELRO is writable only while the loader applies relocations. The
            // loaded game treats these const tables as immutable, not state.
            if(((h.p_type==PT_LOAD&&(h.p_flags&PF_R)&&!(h.p_flags&PF_W))||
                h.p_type==PT_GNU_RELRO)&&h.p_memsz)
                context.ranges->push_back({"game-image/"+std::to_string(i),
                    reinterpret_cast<const void*>(info->dlpi_addr+h.p_vaddr),size_t(h.p_memsz),bool(h.p_flags&PF_X)});
        }
        return 1;
    },&context);
#endif
    return result;
}
}
#endif
