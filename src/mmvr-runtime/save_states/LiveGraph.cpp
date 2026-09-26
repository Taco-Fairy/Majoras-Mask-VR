#include "LiveGraph.h"
#include <algorithm>
#include <cstring>
#include <set>
namespace mmvr::states {
Snapshot CaptureGraph(const Identity& identity,uint64_t tick,const std::vector<LiveBlock>& live,
                      const std::vector<LiveSymbol>& symbols) {
    struct Range { uintptr_t address; uint64_t bytes; ReferenceKind kind; std::string id; };
    if(live.empty() || live.size()>MaxBlocks) throw Error("Invalid live block count");
    uint64_t total=0;
    for(const auto& block:live) {
        if(block.bytes.size()>MaxArchiveBytes-total) throw Error("Live state exceeds archive limit");
        total+=block.bytes.size();
    }
    std::vector<Range> owned, assets;
    std::map<uintptr_t,Range> functions;
    std::set<std::pair<ReferenceKind,std::string>> identities;
    auto add=[&](const void* address,uint64_t bytes,ReferenceKind kind,const std::string& id) {
        const auto start=reinterpret_cast<uintptr_t>(address);
        if(!start || bytes>UINTPTR_MAX-start || (kind!=ReferenceKind::Function && !bytes) ||
           (kind==ReferenceKind::Function && bytes)) throw Error("Invalid live state range: "+id);
        if(!identities.emplace(kind,id).second)throw Error("Duplicate live state symbol: "+id);
        Range value{start,bytes,kind,id};
        if(kind==ReferenceKind::Function) {
            auto [it,inserted]=functions.emplace(start,value);
            // Identical-code folding aliases must be independent of input order.
            if(!inserted && id<it->second.id)it->second=std::move(value);
        } else (kind==ReferenceKind::Owned?owned:assets).push_back(std::move(value));
    };
    for(const auto& block:live) add(block.bytes.data(),block.bytes.size(),ReferenceKind::Owned,block.id);
    for(const auto& symbol:symbols) {
        if(symbol.kind!=ReferenceKind::Asset && symbol.kind!=ReferenceKind::Function)
            throw Error("External symbol cannot own gameplay memory");
        add(symbol.address,symbol.bytes,symbol.kind,symbol.id);
    }
    const auto order=[](const Range& a,const Range& b) {
        if(a.address!=b.address)return a.address<b.address;
        if(a.bytes!=b.bytes)return a.bytes>b.bytes; // containing range before children
        return a.id<b.id; // identical range aliases have a stable canonical name
    };
    std::sort(owned.begin(),owned.end(),order);
    for(size_t i=0;i<owned.size();++i) {
        const auto& range=owned[i];
        if(i && range.address<owned[i-1].address+owned[i-1].bytes)
            throw Error("Overlapping live state ownership: "+range.id+" / "+owned[i-1].id);
        if(functions.contains(range.address))throw Error("Ambiguous function/data ownership: "+range.id);
    }
    std::sort(assets.begin(),assets.end(),order);
    assets.erase(std::unique(assets.begin(),assets.end(),[](const Range& a,const Range& b) {
        return a.address==b.address && a.bytes==b.bytes;
    }),assets.end());
    // External ranges form a containment forest: a whole image may contain a
    // named table, which may contain a named subobject. Crossing is ambiguous.
    constexpr size_t none=SIZE_MAX;
    std::vector<size_t> parents(assets.size(),none), stack;
    for(size_t i=0;i<assets.size();++i) {
        const auto& range=assets[i];
        if(functions.contains(range.address))throw Error("Ambiguous function/data ownership: "+range.id);
        while(!stack.empty() && range.address>=assets[stack.back()].address+assets[stack.back()].bytes)
            stack.pop_back();
        if(!stack.empty()) {
            const auto parent=stack.back();
            if(range.address+range.bytes>assets[parent].address+assets[parent].bytes)
                throw Error("Crossing external state ranges: "+range.id+" / "+assets[parent].id);
            parents[i]=parent;
        }
        stack.push_back(i);
    }
    auto lastStart=[](const std::vector<Range>& ranges,uintptr_t address) {
        return std::upper_bound(ranges.begin(),ranges.end(),address,
            [](uintptr_t value,const Range& range){return value<range.address;});
    };
    auto targetFor=[&](uintptr_t address,PointerType type)->const Range* {
        if(type==PointerType::Function) {
            auto found=functions.find(address);return found==functions.end()?nullptr:&found->second;
        }
        // Mutable owners always win over immutable image registrations, including
        // an exact one-past pointer. Never convert owned gameplay into an asset.
        const auto owner=lastStart(owned,address);
        if(owner!=owned.begin()) {
            const auto& before=*std::prev(owner);
            if(address-before.address<=before.bytes)return &before;
        }
        const auto after=lastStart(assets,address);
        if(after==assets.begin())return nullptr;
        auto index=size_t(std::prev(after)-assets.begin());
        while(index!=none) {
            const auto& range=assets[index];
            if(address-range.address<=range.bytes)return &range;
            index=parents[index];
        }
        return nullptr;
    };
    Snapshot result{identity,tick,{}};
    for(const auto& source:live) {
        Block block{source.id,source.schema,Bytes(source.bytes.begin(),source.bytes.end()),{}};
        std::set<uint64_t> offsets;
        for(const auto& field:source.pointers) {
            if(field.at>source.bytes.size() || sizeof(uintptr_t)>source.bytes.size()-field.at)
                throw Error("Pointer field outside block: "+source.id+"."+field.name);
            auto next=offsets.lower_bound(field.at);
            if((next!=offsets.end() && *next<field.at+sizeof(uintptr_t)) ||
               (next!=offsets.begin() && *std::prev(next)+sizeof(uintptr_t)>field.at))
                throw Error("Overlapping pointer schema: "+source.id+"."+field.name);
            offsets.insert(field.at);
            if(field.type!=PointerType::Data&&!field.literals.empty())
                throw Error("Only data fields may have literal sentinels: "+field.name);
            if(field.type==PointerType::Transient) {
                std::fill_n(block.bytes.data()+field.at,sizeof(uintptr_t),uint8_t(0));
                continue;
            }
            uintptr_t pointer=0;std::memcpy(&pointer,source.bytes.data()+field.at,sizeof(pointer));
            if(!pointer||std::find(field.literals.begin(),field.literals.end(),pointer)!=field.literals.end())continue;
            const Range* target=targetFor(pointer,field.type);
            if(!target) throw Error("Unregistered live pointer: "+source.id+"."+field.name);
            SetReference(block,{field.at,target->kind,target->id,pointer-target->address});
        }
        result.blocks.push_back(std::move(block));
    }
    Validate(result);return result;
}
}
