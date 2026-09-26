#include "Ownership.h"
#include <algorithm>
#include <cstring>
namespace mmvr::states {
void Ownership::Own(std::string id, uint32_t schema, std::span<uint8_t> memory) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(memory.data());
    if (!schema || !address || memory.empty() || memory.size() > UINTPTR_MAX-address)
        throw Error("Invalid owned native region: " + id);
    for (const auto& block : blocks) {
        uintptr_t start = reinterpret_cast<uintptr_t>(block.bytes.data());
        if (block.id==id || (address < start+block.bytes.size() && start < address+memory.size()))
            throw Error("Duplicate/overlapping owned native region: " + id);
    }
    destinations.push_back({id,schema,memory});
    blocks.push_back({std::move(id),schema,memory,{}});
}
void Ownership::Symbol(LiveSymbol symbol) {
    for (const auto& old : symbols) if (old.kind==symbol.kind && old.id==symbol.id) {
        if (old.address!=symbol.address || old.bytes!=symbol.bytes)
            throw Error("Native symbol changed ownership: " + symbol.id);
        return;
    }
    symbols.push_back(std::move(symbol));
}
void Ownership::Pointer(const void* fieldAddress, PointerType type, std::string name, std::vector<uintptr_t> literals) {
    if(type!=PointerType::Data&&!literals.empty())throw Error("Only data fields may have literal sentinels");
    std::sort(literals.begin(),literals.end());
    literals.erase(std::unique(literals.begin(),literals.end()),literals.end());
    const uintptr_t address=reinterpret_cast<uintptr_t>(fieldAddress);
    for (auto& block : blocks) {
        uintptr_t start=reinterpret_cast<uintptr_t>(block.bytes.data());
        if (address<start || address-start>=block.bytes.size()) continue;
        const size_t offset=address-start;
        if (sizeof(uintptr_t)>block.bytes.size()-offset) throw Error("Pointer straddles native owner: " + name);
        for (const auto& old : block.pointers) {
            if (old.at==offset && old.type==type && old.literals==literals) return; // Same typed view via another actor link.
            if (old.at<offset+sizeof(uintptr_t) && offset<old.at+sizeof(uintptr_t))
                throw Error("Conflicting native pointer descriptions: " + name);
        }
        block.pointers.push_back({offset,type,std::move(name),std::move(literals)});
        return;
    }
    throw Error("Pointer field has no native owner: " + name);
}
Snapshot Ownership::Capture(const Identity& identity,uint64_t tick) const {
    return CaptureGraph(identity,tick,blocks,symbols);
}
RestorePlan Ownership::Prepare(const Snapshot& snapshot,const Resolver& resolve) const {
    // Layouts must agree independently of archive CRC: a syntactically valid
    // archive is not permitted to turn a scalar field into a relocated pointer.
    for (const auto& saved : snapshot.blocks) {
        auto it=std::find_if(blocks.begin(),blocks.end(),[&](const auto& block){return block.id==saved.id;});
        if (it==blocks.end()) throw Error("Unknown native region: "+saved.id);
        for (const auto& reference : saved.references) {
            auto field=std::find_if(it->pointers.begin(),it->pointers.end(),[&](const auto& pointer){return pointer.at==reference.at;});
            if (field==it->pointers.end() || field->type==PointerType::Transient || (field->type==PointerType::Function)!=(reference.kind==ReferenceKind::Function))
                throw Error("Native reference does not match typed layout: "+saved.id);
        }
        for (const auto& field : it->pointers) {
            if(field.at>saved.bytes.size()||sizeof(uintptr_t)>saved.bytes.size()-field.at)
                throw Error("Native pointer field outside saved region: "+saved.id);
            uintptr_t value=0;std::memcpy(&value,saved.bytes.data()+field.at,sizeof(value));
            if(value&&std::find(field.literals.begin(),field.literals.end(),value)==field.literals.end())
                throw Error("Native pointer bytes were not encoded symbolically: "+saved.id);
        }
    }
    return RestorePlan(snapshot,destinations,resolve);
}
}
