#include "Ownership.h"
#include "Fingerprint.h"
#include "RestorePlan.h"
#include "Archive.h"
#include "LiveGraph.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace mmvr::states;
namespace {
int failures=0;
void Check(bool good,const char* name){std::cout<<(good?"PASS ":"FAIL ")<<name<<'\n';failures+=!good;}
template<class F> void Reject(F fn,const char* name){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}Check(rejected,name);}
Identity identity{"build-99-test","rom-and-mod-hash","test-64-le"};
Snapshot Sample(){
    Snapshot s{identity,843,{}};
    Block enemy{"actor/enemy/17",1,Bytes(32,0),{}};enemy.bytes[0]=31;
    Block arrow{"actor/arrow/23",1,Bytes(32,0),{}};arrow.bytes[0]=77;
    SetReference(enemy,{8,ReferenceKind::Owned,"actor/arrow/23",16});
    SetReference(arrow,{8,ReferenceKind::Owned,"actor/enemy/17",0});
    SetReference(arrow,{16,ReferenceKind::Asset,"objects/arrow/model",4});
    SetReference(arrow,{24,ReferenceKind::Function,"EnArrow_Update/v1",0});
    s.blocks={std::move(enemy),std::move(arrow)};return s;
}
uintptr_t Pointer(void* memory,size_t at){uintptr_t value;std::memcpy(&value,static_cast<uint8_t*>(memory)+at,sizeof(value));return value;}
struct Prepared final:PreparedComponent {
    int* live;int value;Prepared(int* p,int v):live(p),value(v){}
    void Commit() noexcept override{*live=value;}
};
}
int main(int argc,char** argv){
 try {
    if(argc!=2)throw Error("Supply isolated test directory");
    std::filesystem::path root=argv[1];Store store(root);
    {
        Fingerprint hash;
        Check(hash.Hex()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA-256 empty vector");
        hash.Add("abc");
        Check(hash.Hex()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 short vector");
        hash=Fingerprint{};hash.Add("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
        Check(hash.Hex()=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA-256 padding boundary");
        hash=Fingerprint{};for(unsigned i=0;i<1000;++i)hash.Add(std::string(1000,'a'));
        Check(hash.Hex()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA-256 streamed million-byte vector");
        Fingerprint framedA,framedB;framedA.Field("a");framedA.Field("bc");framedB.Field("ab");framedB.Field("c");
        Check(framedA.Hex()!=framedB.Hex(),"identity fields have unambiguous boundaries");
    }
    auto original=Sample();auto bytes=Encode(original);auto decoded=Decode(bytes,identity);
    Check(Encode(decoded)==bytes,"binary archive round trip");
    auto foreign=original;foreign.identity={"older-build","other-assets","older-abi"};
    const auto foreignBytes=Encode(foreign);const auto inspected=DecodeUnbound(foreignBytes);
    Check(inspected.identity==foreign.identity && inspected.tick==foreign.tick &&
          inspected.blocks[0].bytes==foreign.blocks[0].bytes,"unbound inspection retains foreign identity and payload");
    Reject([&]{Decode(foreignBytes,identity);},"strict decoder still rejects foreign archive");
    for(size_t i=0;i<bytes.size();++i){auto corrupt=bytes;corrupt[i]^=0x80;
        try{DecodeUnbound(corrupt);++failures;std::cout<<"FAIL unbound corruption accepted at "<<i<<'\n';}catch(const std::exception&) {}}
    for(size_t i=0;i<bytes.size();++i){try{DecodeUnbound(std::span(bytes).first(i));++failures;}catch(const std::exception&) {}}
    for(size_t i=0;i<bytes.size();++i){auto corrupt=bytes;corrupt[i]^=0x80;try{Decode(corrupt,identity);++failures;std::cout<<"FAIL corruption accepted at "<<i<<'\n';}catch(const std::exception&) {}}
    Check(failures==0,"every single-byte corruption rejected");
    for(size_t i=0;i<bytes.size();++i){try{Decode(std::span(bytes).first(i),identity);++failures;}catch(const std::exception&) {}}
    Check(failures==0,"every truncation rejected");
    auto other=identity;other.build="other-build";Reject([&]{Decode(bytes,other);},"build mismatch rejected");
    other=identity;other.assets="other-pack";Reject([&]{Decode(bytes,other);},"content mismatch rejected");
    other=identity;other.abi="other-platform";Reject([&]{Decode(bytes,other);},"platform mismatch rejected");
    auto bad=original;bad.blocks.push_back(bad.blocks.front());Reject([&]{Encode(bad);},"duplicate block rejected");
    bad=original;bad.blocks[0].references[0].target="missing";Reject([&]{Encode(bad);},"dangling actor link rejected");
    bad=original;bad.blocks[0].references[0].offset=33;Reject([&]{Encode(bad);},"reference target overflow rejected");
    bad=original;bad.blocks[0].references[0].at=30;Reject([&]{Encode(bad);},"reference field overflow rejected");
    bad=original;bad.blocks[0].references.push_back(bad.blocks[0].references[0]);Reject([&]{Encode(bad);},"duplicate pointer field rejected");
    bad=original;bad.blocks[0].bytes[8]=42;Reject([&]{Encode(bad);},"process-address bytes rejected");
    std::array<uint8_t,16> asset{},function{};
    auto resolver=[&](ReferenceKind kind,const std::string& name)->ExternalRange {
        if(kind==ReferenceKind::Asset&&name=="objects/arrow/model")return {asset.data(),asset.size()};
        if(kind==ReferenceKind::Function&&name=="EnArrow_Update/v1")return {function.data(),0};
        return {};
    };
    Graph first(decoded,resolver),second(decoded,resolver);
    Check(first.Address("actor/enemy/17")!=second.Address("actor/enemy/17"),"restored graph uses new allocations");
    Check(Pointer(second.Address("actor/enemy/17"),8)==reinterpret_cast<uintptr_t>(second.Address("actor/arrow/23"))+16,"interior actor link relocated");
    Check(Pointer(second.Address("actor/arrow/23"),8)==reinterpret_cast<uintptr_t>(second.Address("actor/enemy/17")),"cyclic actor link relocated");
    Check(Pointer(second.Address("actor/arrow/23"),16)==reinterpret_cast<uintptr_t>(asset.data())+4,"asset link resolved by identity");
    Check(Pointer(second.Address("actor/arrow/23"),24)==reinterpret_cast<uintptr_t>(function.data()),"function link resolved by identity");
    Reject([&]{Graph missing(decoded,[](auto,auto&){return ExternalRange{};});},"missing resource rejects candidate graph");
    for(int slot=1;slot<=3;++slot){auto s=original;s.tick=slot*100;store.Save(slot,s);Check(store.Load(slot,identity).tick==uint64_t(slot*100),"three independent durable slots");}
    Reject([&]{store.Save(0,original);},"slot zero rejected");Reject([&]{store.Load(4,identity);},"slot four rejected");
    store.Save(3,foreign);
    Check(store.LoadUnbound(3).identity==foreign.identity,"unbound store reads foreign identity");
    Reject([&]{store.Load(3,identity);},"strict store rejects foreign identity after unbound read");
    Check(store.Load(3,foreign.identity).tick==foreign.tick,"unbound inspection leaves original slot intact");
    Reject([&]{store.LoadUnbound(0);},"unbound slot zero rejected");
    Reject([&]{store.LoadUnbound(4);},"unbound slot four rejected");
    bad=original;bad.blocks[0].schema=0;Reject([&]{store.Save(1,bad);},"invalid replacement rejected before write");
    Check(store.Load(1,identity).tick==100,"failed replacement preserves previous slot");
    {std::ofstream(root/"save-states/slot-2.mmstate",std::ios::binary|std::ios::trunc)<<"bad";}
    Reject([&]{store.Load(2,identity);},"corrupt on-disk slot rejected");
    Reject([&]{store.LoadUnbound(2);},"corrupt unbound on-disk slot rejected");
    int liveEnemy=0,liveArrow=0;
    std::vector<Component> components{
      {"actor/enemy/17",1,{},[&](const Block& b){return std::make_unique<Prepared>(&liveEnemy,b.bytes[0]);}},
      {"actor/arrow/23",1,{},[&](const Block& b){return std::make_unique<Prepared>(&liveArrow,b.bytes[0]);}}
    };
    auto failing=components;failing[1].prepare=[](const Block&)->std::unique_ptr<PreparedComponent>{throw Error("cannot prepare");};
    Reject([&]{Transaction tx(original,failing,{"actor/enemy/17","actor/arrow/23"});},"failed preparation rejects whole restore");
    Check(liveEnemy==0&&liveArrow==0,"failed preparation leaves live state unchanged");
    Reject([&]{Transaction tx(original,components,{"player","actor/enemy/17","actor/arrow/23"});},"missing required gameplay component rejected");
    Transaction tx(original,components,{"actor/enemy/17","actor/arrow/23"});
    Check(liveEnemy==0&&liveArrow==0,"preparation does not mutate live state");
    tx.Commit();Check(liveEnemy==31&&liveArrow==77,"commit restores every prepared component");
    liveEnemy=99;tx.Commit();Check(liveEnemy==99,"restore cannot commit twice");
    {
        struct Node { Node* next; uint8_t* mesh; uintptr_t function; uintptr_t scalar; };
        Node a{},b{};std::array<uint8_t,24> mesh{};
        a={&b,mesh.data()+3,0x2000,reinterpret_cast<uintptr_t>(&b)};
        b={&a,mesh.data()+mesh.size(),0x2000,41};
        auto node=[&](const char* id,Node& value){return LiveBlock{id,1,
            {reinterpret_cast<const uint8_t*>(&value),sizeof(value)},
            {{offsetof(Node,next),PointerType::Data,"next"},{offsetof(Node,mesh),PointerType::Data,"mesh"},
             {offsetof(Node,function),PointerType::Function,"update"}}};};
        std::vector<LiveBlock> live{node("a",a),node("b",b)};
        std::vector<LiveSymbol> symbols{{ReferenceKind::Asset,"mesh",mesh.data(),mesh.size()},
            {ReferenceKind::Function,"update",reinterpret_cast<void*>(0x2000),0},
            {ReferenceKind::Function,"alias",reinterpret_cast<void*>(0x2000),0}};
        auto capture=CaptureGraph(original.identity,93,live,symbols);
        uintptr_t untouched=0;std::memcpy(&untouched,capture.blocks[0].bytes.data()+offsetof(Node,scalar),sizeof(untouched));
        Check(untouched==a.scalar,"scalar values are never guessed to be pointers");
        Check(capture.blocks[0].references[2].target=="alias","folded functions use a stable symbolic alias");
        Graph restored(capture,[&](ReferenceKind kind,const std::string&){
            return kind==ReferenceKind::Asset?ExternalRange{mesh.data(),mesh.size()}:ExternalRange{reinterpret_cast<void*>(0x4000),0};});
        auto* restoredA=static_cast<Node*>(restored.Address("a"));auto* restoredB=static_cast<Node*>(restored.Address("b"));
        Check(restoredA->next==restoredB && restoredB->next==restoredA,"live cyclic graph relocates");
        Check(restoredA->mesh==mesh.data()+3 && restoredB->mesh==mesh.data()+mesh.size(),"asset interior and one-past pointers relocate");
        Check(restoredA->function==0x4000,"function addresses resolve after restart");
        a.next=reinterpret_cast<Node*>(0x1234);
        Reject([&]{CaptureGraph(original.identity,0,live,symbols);},"unowned live pointer cannot be saved");
        a.next=nullptr;auto nullable=CaptureGraph(original.identity,0,live,symbols);
        Check(nullable.blocks[0].references.size()==2,"null typed pointer stays null");
        live[0].pointers.push_back({offsetof(Node,next),PointerType::Data,"duplicate"});
        Reject([&]{CaptureGraph(original.identity,0,live,symbols);},"overlapping null schema fields rejected");
        live={node("a",a),node("alias",a)};
        Reject([&]{CaptureGraph(original.identity,0,live,symbols);},"overlapping owned blocks rejected");
    }
    {
        struct Link { Link* next; int action; };
        Link beforeA{}, beforeB{};
        beforeA={&beforeB,71}; beforeB={&beforeA,29};
        auto block=[](const char* id, Link& value){return LiveBlock{id,7,
            {reinterpret_cast<const uint8_t*>(&value),sizeof(value)},
            {{offsetof(Link,next),PointerType::Data,"next"}}};};
        auto saved=CaptureGraph(identity,17,{block("native/a",beforeA),block("native/b",beforeB)},{});
        Link afterA{},afterB{};
        auto bind=[](const char* id, Link& value){return RestoreBinding{id,7,
            {reinterpret_cast<uint8_t*>(&value),sizeof(value)}};};
        std::vector<RestoreBinding> bindings{bind("native/a",afterA),bind("native/b",afterB)};
        RestorePlan plan(saved,bindings,{});
        Check(afterA.next==nullptr&&afterA.action==0,"native restore preparation cannot change live memory");
        plan.Commit();
        Check(afterA.next==&afterB&&afterB.next==&afterA&&afterA.action==71&&afterB.action==29,
              "native restore relocates cycles to final engine addresses");
        afterA.action=8;plan.Commit();Check(afterA.action==8,"native restore commit is one shot");
        bindings[1].schema=8;
        Reject([&]{RestorePlan failed(saved,bindings,{});},"all native layouts must match before commit");
        Check(afterA.action==8,"failed native preparation preserves live engine state");
        bindings[1]=bind("native/b",afterA);
        Reject([&]{RestorePlan overlap(saved,bindings,{});},"aliased native destinations rejected");
    }
    {
        Snapshot snapshot{identity,0,{{"a",1,Bytes(16,1),{}},{"b",1,Bytes(16,2),{}},{"c",1,Bytes(16,3),{}}}};
        std::array<uint8_t,64> memory{};
        std::vector<RestoreBinding> bindings{{"c",1,{memory.data()+32,16}},
            {"a",1,{memory.data(),16}},{"b",1,{memory.data()+16,16}}};
        RestorePlan adjacent(snapshot,bindings,{});adjacent.Commit();
        Check(memory[0]==1&&memory[16]==2&&memory[32]==3,"unsorted adjacent restore ranges accepted");
        auto invalid=bindings;invalid[0].destination={memory.data()+8,16};
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"overlap between unsorted restore ranges rejected");
        invalid=bindings;invalid[0].destination={memory.data()+15,16};
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"one-byte restore overlap rejected");
        invalid=bindings;invalid[0].destination={memory.data()+4,4};
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"nested restore destination rejected");
        invalid=bindings;invalid[0].id="a";
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"duplicate restore binding rejected");
        invalid=bindings;invalid[0].destination={};
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"empty restore destination rejected");
        invalid=bindings;invalid[0].destination={reinterpret_cast<uint8_t*>(UINTPTR_MAX-7),16};
        Reject([&]{RestorePlan plan(snapshot,invalid,{});},"overflowing restore destination rejected");
        Check(memory[0]==1&&memory[16]==2&&memory[32]==3,"rejected ranges never mutate live state");
    }
    {
        struct ArenaImage { uint64_t counter; void* actor; uint64_t untouched; };
        ArenaImage source{73,nullptr,0xdeadbeef}, destination{};
        source.actor=&source.counter;
        Ownership capture;
        capture.Own("native/arena",1,{reinterpret_cast<uint8_t*>(&source),sizeof(source)});
        capture.Pointer(&source.actor,PointerType::Data,"actor.counter");
        capture.Pointer(&source.actor,PointerType::Data,"alias.counter");
        auto snapshot=capture.Capture(identity,4);
        Ownership restore;
        restore.Own("native/arena",1,{reinterpret_cast<uint8_t*>(&destination),sizeof(destination)});
        restore.Pointer(&destination.actor,PointerType::Data,"actor.counter");
        auto plan=restore.Prepare(snapshot,{});plan.Commit();
        Check(destination.actor==&destination.counter&&destination.counter==73&&destination.untouched==0xdeadbeef,
              "typed actor views relocate inside their native arena owner");
        auto bad=snapshot;bad.blocks[0].references[0].at=offsetof(ArenaImage,untouched);
        bad.blocks[0].bytes[offsetof(ArenaImage,untouched)]=0;
        Reject([&]{restore.Prepare(bad,{});},"archive cannot relocate an undescribed scalar");
        Reject([&]{capture.Pointer(&source.actor,PointerType::Function,"conflict");},"conflicting pointer variants rejected");
    }
    {
        struct Tagged { uintptr_t layer,instrument; void* transient; uint64_t action; };
        Tagged source{UINTPTR_MAX,2,reinterpret_cast<void*>(0x1234),53},destination{};
        auto describe=[](Ownership& owner,Tagged& value) {
            owner.Own("native/tags",1,{reinterpret_cast<uint8_t*>(&value),sizeof(value)});
            owner.Pointer(&value.layer,PointerType::Data,"layer",{UINTPTR_MAX});
            owner.Pointer(&value.instrument,PointerType::Data,"instrument",{1,2});
            owner.Pointer(&value.transient,PointerType::Transient,"frame-input");
        };
        Ownership capture,restore;describe(capture,source);describe(restore,destination);
        auto snapshot=capture.Capture(identity,7);
        auto plan=restore.Prepare(snapshot,{});plan.Commit();
        Check(destination.layer==UINTPTR_MAX&&destination.instrument==2&&!destination.transient&&destination.action==53,
              "native sentinel tags preserved and expired frame pointers cleared");
        Check(source.transient==reinterpret_cast<void*>(0x1234),"capture never alters live transient input");
        auto invalid=snapshot;uintptr_t unknown=3;
        std::memcpy(invalid.blocks[0].bytes.data()+offsetof(Tagged,instrument),&unknown,sizeof(unknown));
        Reject([&]{restore.Prepare(invalid,{});},"unknown pointer literal rejected before restore");
        invalid=snapshot;
        SetReference(invalid.blocks[0],{offsetof(Tagged,transient),ReferenceKind::Owned,"native/tags",0});
        Reject([&]{restore.Prepare(invalid,{});},"archive cannot restore an expired frame-input address");
    }
    {
        std::array<uint8_t,32> adjoining{};
        uintptr_t reference=reinterpret_cast<uintptr_t>(adjoining.data()+16);
        LiveBlock pointer{"pointer",1,{reinterpret_cast<uint8_t*>(&reference),sizeof(reference)},{{0,PointerType::Data,"edge"}}};
        std::vector<LiveSymbol> ranges{{ReferenceKind::Asset,"left",adjoining.data(),16},
                                      {ReferenceKind::Asset,"right",adjoining.data()+16,16}};
        auto state=CaptureGraph(identity,0,{pointer},ranges);
        Check(state.blocks[0].references[0].target=="right"&&state.blocks[0].references[0].offset==0,
              "adjacent range start wins over preceding one-past endpoint");
        reference=reinterpret_cast<uintptr_t>(adjoining.data()+32);
        state=CaptureGraph(identity,0,{pointer},ranges);
        Check(state.blocks[0].references[0].target=="right"&&state.blocks[0].references[0].offset==16,
              "last range permits its exact one-past endpoint");
        ranges[1].address=adjoining.data()+15;
        Reject([&]{CaptureGraph(identity,0,{pointer},ranges);},"partially overlapping external ranges rejected");
        ranges[1].address=adjoining.data()+16;
        ranges.push_back({ReferenceKind::Function,"conflict",adjoining.data(),0});
        Reject([&]{CaptureGraph(identity,0,{pointer},ranges);},"function and data start cannot alias");
        ranges.pop_back();ranges.push_back(ranges.front());
        Reject([&]{CaptureGraph(identity,0,{pointer},ranges);},"duplicate external identities rejected");
    }
    {
        std::array<uint8_t,96> image{};
        uintptr_t reference=reinterpret_cast<uintptr_t>(image.data()+24);
        LiveBlock pointer{"pointer",1,{reinterpret_cast<uint8_t*>(&reference),sizeof(reference)},{{0,PointerType::Data,"nested"}}};
        std::vector<LiveSymbol> ranges{{ReferenceKind::Asset,"game-image/1",image.data(),image.size()},
            {ReferenceKind::Asset,"table",image.data()+16,32},
            {ReferenceKind::Asset,"element",image.data()+24,8}};
        const auto target=[&]() {return CaptureGraph(identity,0,{pointer},ranges).blocks[0].references[0];};
        auto ref=target();Check(ref.target=="element"&&ref.offset==0,"most specific nested symbol wins");
        reference=reinterpret_cast<uintptr_t>(image.data()+32);
        ref=target();Check(ref.target=="element"&&ref.offset==8,"nested symbol one-past is retained");
        ranges.push_back({ReferenceKind::Asset,"next",image.data()+32,8});
        ref=target();Check(ref.target=="next"&&ref.offset==0,"adjacent child start beats previous child end");
        reference=reinterpret_cast<uintptr_t>(image.data()+45);
        Check(target().target=="table","after a child ends lookup returns to containing table");
        reference=reinterpret_cast<uintptr_t>(image.data()+80);
        Check(target().target=="game-image/1","image is only fallback outside named objects");
        ranges.push_back({ReferenceKind::Asset,"crossing",image.data()+40,16});
        Reject([&]{target();},"crossing nested object boundaries rejected");ranges.pop_back();
        ranges.push_back({ReferenceKind::Asset,"a-element",image.data()+24,8});
        reference=reinterpret_cast<uintptr_t>(image.data()+25);
        Check(target().target=="a-element","equal asset aliases use stable lexical identity");
        std::reverse(ranges.begin(),ranges.end());
        Check(target().target=="a-element","asset alias identity independent of registration order");
        LiveBlock mutableBlock{"mutable",1,{image.data()+24,8},{}};
        auto state=CaptureGraph(identity,0,{pointer,mutableBlock},ranges);
        Check(state.blocks[0].references[0].target=="mutable"&&state.blocks[0].references[0].kind==ReferenceKind::Owned,
              "owned block wins over nested assets and identical alias ranges");
        reference=reinterpret_cast<uintptr_t>(image.data()+32);
        state=CaptureGraph(identity,0,{pointer,mutableBlock},ranges);
        Check(state.blocks[0].references[0].target=="mutable"&&state.blocks[0].references[0].offset==8,
              "owned one-past wins over adjacent external range");
        ranges.push_back({ReferenceKind::Asset,"same-start-narrower",image.data(),4});
        reference=reinterpret_cast<uintptr_t>(image.data()+1);
        Check(target().target=="same-start-narrower","shared base selects smallest external range");
        reference=reinterpret_cast<uintptr_t>(image.data()+96);
        Check(target().target=="game-image/1"&&target().offset==96,"outer one-past remains valid");
    }
    std::cout<<"failures="<<failures<<'\n';return failures?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
