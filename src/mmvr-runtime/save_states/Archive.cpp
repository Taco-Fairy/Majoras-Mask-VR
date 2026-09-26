#include "Archive.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <mutex>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace mmvr::states {
namespace {
constexpr uint32_t Version = 1;
constexpr uint8_t Magic[8] = {'M','M','V','R','S','T','A','T'};
constexpr auto CrcTable=[] {
    std::array<uint32_t,256> table{};
    for(uint32_t value=0;value<table.size();++value) {
        uint32_t remainder=value;
        for(int bit=0;bit<8;++bit)remainder=(remainder>>1)^(0xedb88320u & (0u-(remainder&1u)));
        table[value]=remainder;
    }
    return table;
}();
uint32_t Crc(std::span<const uint8_t> data) {
    uint32_t crc=~0u;
    for(auto byte:data)crc=(crc>>8)^CrcTable[(crc^byte)&255u];
    return ~crc;
}
struct Writer {
    Bytes bytes;
    void Number(uint64_t value, int width) { if(bytes.size()>MaxArchiveBytes-uint64_t(width)) throw Error("State exceeds archive limit"); for(int i=0;i<width;++i) bytes.push_back(uint8_t(value>>(8*i))); }
    void Data(std::span<const uint8_t> data) {
        if(bytes.size()>MaxArchiveBytes||data.size()>MaxArchiveBytes-bytes.size()) throw Error("State exceeds archive limit");
        bytes.insert(bytes.end(),data.begin(),data.end());
    }
    void Text(const std::string& text) { Number(text.size(),4); Data({reinterpret_cast<const uint8_t*>(text.data()),text.size()}); }
};
struct Reader {
    std::span<const uint8_t> data; size_t offset=0;
    std::span<const uint8_t> Data(uint64_t count) {
        if(count>data.size()-offset) throw Error("Truncated state");
        auto part=data.subspan(offset,size_t(count));offset+=size_t(count);return part;
    }
    uint64_t Number(int width) { auto part=Data(width);uint64_t value=0;for(int i=0;i<width;++i)value|=uint64_t(part[i])<<(8*i);return value; }
    std::string Text() { auto size=Number(4);if(size>4096)throw Error("State name too long");auto part=Data(size);return {reinterpret_cast<const char*>(part.data()),part.size()}; }
};
void Name(const std::string& name) { if(name.empty()||name.size()>4096||name.find('\0')!=std::string::npos)throw Error("Invalid state identifier"); }
std::mutex storeMutex;
void WriteAtomic(const std::filesystem::path& destination, std::span<const uint8_t> bytes) {
    static std::atomic<uint64_t> sequence{0};
    auto temporary=destination;
#ifdef _WIN32
    auto process=GetCurrentProcessId();
#else
    auto process=getpid();
#endif
    temporary += ".pending-"+std::to_string(process)+"-"+std::to_string(++sequence);
    if(std::filesystem::is_symlink(destination)||std::filesystem::exists(temporary)) throw Error("Unsafe state destination");
#ifdef _WIN32
    FILE* file=_wfopen(temporary.c_str(),L"wbx");
#else
    FILE* file=std::fopen(temporary.c_str(),"wbx");
#endif
    if(!file)throw Error("Cannot create state file");
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();
    ok=std::fflush(file)==0&&ok;
#ifdef _WIN32
    ok=_commit(_fileno(file))==0&&ok;
#else
    ok=fsync(fileno(file))==0&&ok;
#endif
    ok=std::fclose(file)==0&&ok;
    if(ok){
#ifdef _WIN32
        ok=MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=std::rename(temporary.c_str(),destination.c_str())==0;
#endif
    }
    if(!ok){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw Error("State write failed; previous slot retained");}
#ifndef _WIN32
    int directory=open(destination.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
    if(directory<0)throw Error("State written but directory durability could not be confirmed");
    int status=fsync(directory);close(directory);
    if(status)throw Error("State written but directory durability could not be confirmed");
#endif
}
}
void SetReference(Block& block, Reference ref) {
    if(ref.at>block.bytes.size()||sizeof(uintptr_t)>block.bytes.size()-ref.at)throw Error("Pointer field outside block");
    std::fill_n(block.bytes.begin()+size_t(ref.at),sizeof(uintptr_t),uint8_t(0));
    block.references.push_back(std::move(ref));
}
void Validate(const Snapshot& state) {
    Name(state.identity.build);Name(state.identity.assets);Name(state.identity.abi);
    if(state.blocks.empty()||state.blocks.size()>MaxBlocks)throw Error("Invalid state block count");
    std::map<std::string,const Block*> blocks;
    uint64_t total=0,referenceCount=0;
    for(const auto& block:state.blocks){
        Name(block.id);
        if(!block.schema||!blocks.emplace(block.id,&block).second)throw Error("Duplicate or unversioned state block");
        if(block.bytes.empty()||block.bytes.size()>MaxArchiveBytes-total)throw Error("Invalid state block size");
        total+=block.bytes.size();referenceCount+=block.references.size();
        if(referenceCount>MaxReferences)throw Error("Too many state references");
    }
    for(const auto& block:state.blocks){
        std::set<uint64_t> positions;
        for(const auto& ref:block.references){
            Name(ref.target);
            if(ref.at>block.bytes.size()||sizeof(uintptr_t)>block.bytes.size()-ref.at)throw Error("Reference field out of bounds");
            auto next=positions.lower_bound(ref.at);
            if((next!=positions.end()&&*next<ref.at+sizeof(uintptr_t))||
               (next!=positions.begin()&&*std::prev(next)+sizeof(uintptr_t)>ref.at))throw Error("Overlapping state references");
            positions.insert(ref.at);
            for(size_t i=0;i<sizeof(uintptr_t);++i)if(block.bytes[size_t(ref.at)+i])throw Error("Serialized reference contains a process address");
            if(ref.kind==ReferenceKind::Owned){
                auto target=blocks.find(ref.target);
                if(target==blocks.end()||ref.offset>target->second->bytes.size())throw Error("Unresolved owned reference");
            }else if(ref.kind!=ReferenceKind::Asset&&ref.kind!=ReferenceKind::Function)throw Error("Unknown reference type");
            if(ref.kind==ReferenceKind::Function&&ref.offset)throw Error("Function references require exact symbolic identity");
        }
    }
}
Bytes Encode(const Snapshot& state) {
    Validate(state);Writer payload;
    payload.Text(state.identity.build);payload.Text(state.identity.assets);payload.Text(state.identity.abi);
    payload.Number(sizeof(uintptr_t),1);payload.Number(state.tick,8);payload.Number(state.blocks.size(),4);
    for(const auto& block:state.blocks){
        payload.Text(block.id);payload.Number(block.schema,4);payload.Number(block.bytes.size(),8);payload.Data(block.bytes);
        payload.Number(block.references.size(),4);
        for(const auto& ref:block.references){payload.Number(ref.at,8);payload.Number(uint8_t(ref.kind),1);payload.Text(ref.target);payload.Number(ref.offset,8);}
    }
    Writer output;output.Data(Magic);output.Number(Version,4);output.Number(payload.bytes.size(),8);output.Number(Crc(payload.bytes),4);output.Data(payload.bytes);return output.bytes;
}
Snapshot DecodeUnbound(std::span<const uint8_t> bytes) {
    if(bytes.size()>MaxArchiveBytes)throw Error("State exceeds archive limit");
    Reader header{bytes};auto magic=header.Data(8);
    if(!std::equal(magic.begin(),magic.end(),Magic)||header.Number(4)!=Version)throw Error("Unknown save-state format");
    auto size=header.Number(8);auto crc=header.Number(4);auto payload=header.Data(size);
    if(header.offset!=bytes.size()||Crc(payload)!=crc)throw Error("Save-state checksum mismatch");
    Reader input{payload};Snapshot result;
    result.identity={input.Text(),input.Text(),input.Text()};
    if(input.Number(1)!=sizeof(uintptr_t))throw Error("State pointer width mismatch");
    result.tick=input.Number(8);auto count=input.Number(4);
    if(!count||count>MaxBlocks)throw Error("Invalid state block count");
    uint64_t references=0;
    for(uint64_t i=0;i<count;++i){
        Block block;block.id=input.Text();block.schema=uint32_t(input.Number(4));
        auto data=input.Data(input.Number(8));block.bytes.assign(data.begin(),data.end());
        auto refs=input.Number(4);references+=refs;if(references>MaxReferences)throw Error("Too many state references");
        for(uint64_t j=0;j<refs;++j){Reference ref;ref.at=input.Number(8);ref.kind=ReferenceKind(input.Number(1));ref.target=input.Text();ref.offset=input.Number(8);block.references.push_back(std::move(ref));}
        result.blocks.push_back(std::move(block));
    }
    if(input.offset!=payload.size())throw Error("Unexpected state trailing data");
    Validate(result);return result;
}
Snapshot Decode(std::span<const uint8_t> bytes, const Identity& expected) {
    auto result=DecodeUnbound(bytes);
    if(result.identity!=expected)throw Error("Save state belongs to a different build, content set or platform");
    return result;
}
Store::Store(std::filesystem::path root):directory(std::move(root)/"save-states") {}
std::filesystem::path Store::Slot(int slot) const {
    if(slot<1||slot>3)throw Error("Save-state slot must be 1, 2 or 3");
    return directory/("slot-"+std::to_string(slot)+".mmstate");
}
void Store::Save(int slot,const Snapshot& state){
    auto path=Slot(slot);auto bytes=Encode(state);std::lock_guard lock(storeMutex);
    if(std::filesystem::is_symlink(directory))throw Error("Save-state directory must not be a link");
    std::filesystem::create_directories(directory);WriteAtomic(path,bytes);
}
Snapshot Store::LoadUnbound(int slot) const {
    auto path=Slot(slot);std::lock_guard lock(storeMutex);
    if(std::filesystem::is_symlink(directory)||std::filesystem::is_symlink(path))throw Error("Save-state path must not be a link");
    auto size=std::filesystem::file_size(path);if(size>MaxArchiveBytes)throw Error("State exceeds archive limit");
    Bytes bytes(size);std::ifstream input(path,std::ios::binary);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(size)))throw Error("Cannot read complete state");
    return DecodeUnbound(bytes);
}
Snapshot Store::Load(int slot,const Identity& expected) const {
    auto result=LoadUnbound(slot);
    if(result.identity!=expected)throw Error("Save state belongs to a different build, content set or platform");
    return result;
}
bool Store::Exists(int slot) const {return std::filesystem::is_regular_file(Slot(slot));}
Graph::Graph(const Snapshot& state,const Resolver& resolve){
    Validate(state);
    for(const auto& block:state.blocks){auto memory=std::make_unique<uint8_t[]>(block.bytes.size());std::memcpy(memory.get(),block.bytes.data(),block.bytes.size());sizes.emplace(block.id,block.bytes.size());blocks.emplace(block.id,std::move(memory));}
    for(const auto& block:state.blocks)for(const auto& ref:block.references){
        uintptr_t pointer;
        if(ref.kind==ReferenceKind::Owned)pointer=reinterpret_cast<uintptr_t>(Address(ref.target))+ref.offset;
        else{auto target=resolve(ref.kind,ref.target);if(!target.address||ref.offset>target.bytes)throw Error("Asset or function unavailable: "+ref.target);
            auto base=reinterpret_cast<uintptr_t>(target.address);if(ref.offset>UINTPTR_MAX-base)throw Error("External reference overflow");pointer=base+ref.offset;}
        std::memcpy(blocks.at(block.id).get()+ref.at,&pointer,sizeof(pointer));
    }
}
void* Graph::Address(const std::string& id) const {auto found=blocks.find(id);if(found==blocks.end())throw Error("Missing state block: "+id);return found->second.get();}
uint64_t Graph::Size(const std::string& id) const {return sizes.at(id);}
Transaction::Transaction(const Snapshot& state,const std::vector<Component>& components,const std::vector<std::string>& required){
    Validate(state);std::map<std::string,const Component*> registry;std::map<std::string,const Block*> saved;
    for(const auto& c:components)if(!c.prepare||!registry.emplace(c.id,&c).second)throw Error("Invalid state component registry");
    for(const auto& b:state.blocks)saved.emplace(b.id,&b);
    for(const auto& id:required)if(!registry.contains(id)||!saved.contains(id))throw Error("Incomplete exact state: "+id);
    for(const auto& b:state.blocks){auto component=registry.find(b.id);if(component==registry.end()||component->second->schema!=b.schema)throw Error("Unsupported state component: "+b.id);}
    for(const auto& b:state.blocks){auto candidate=registry.at(b.id)->prepare(b);if(!candidate)throw Error("State component did not prepare: "+b.id);prepared.push_back(std::move(candidate));}
}
void Transaction::Commit() noexcept {if(committed)return;for(auto& value:prepared)value->Commit();committed=true;}
} // namespace mmvr::states
