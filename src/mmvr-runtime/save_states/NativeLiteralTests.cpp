#include "../../2ship2harkinian/mm/2s2h/VR/NativeStateLiterals.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mmvrgame;
int main() {
    int cases=0;
    const auto check=[&](bool ok,const char* label) {
        if(!ok)throw std::runtime_error(label);
        ++cases;std::cout<<"PASS "<<label<<'\n';
    };
    const auto rejects=[&](const std::string& name,const char* label) {
        bool rejected=false;try {ResolveStateLiteral(name);}catch(const mmvr::states::Error&){rejected=true;}
        check(rejected,label);
    };
    try {
        const char ascii[]="Moon's Tear";
        std::array<NativeModuleRange,1> ranges{{{"image",ascii,sizeof(ascii),false}}};
        auto encode=[&](const void* address){return EncodeStateLiteral(address,ranges);};
        const auto name=encode(ascii);
        check(name&&*name=="literal/4d6f6f6e27732054656172","ASCII stable hex identity");
        const auto first=ResolveStateLiteral(*name);
        check(first.address&&first.bytes==sizeof(ascii)&&!std::memcmp(first.address,ascii,sizeof(ascii)),"ASCII resolves including terminator");
        const char japanese[]={char(0xe3),char(0x82),char(0xbc),char(0xe3),char(0x83),char(0xab),char(0xe3),char(0x83),char(0x80),0};
        ranges[0]={"japanese",japanese,sizeof(japanese),false};
        const auto jp=encode(japanese);
        check(jp&&*jp=="literal/e382bce383abe38380","Japanese bytes preserved without encoding conversion");
        const auto resolvedJp=ResolveStateLiteral(*jp);
        check(resolvedJp.bytes==sizeof(japanese)&&!std::memcmp(resolvedJp.address,japanese,sizeof(japanese)),"Japanese literal round trip");
        ranges[0]={"ascii",ascii,sizeof(ascii),false};
        auto suffix=encode(ascii+7);check(suffix&&*suffix=="literal/54656172","interior suffix encoded independently");
        auto empty=encode(ascii+sizeof(ascii)-1);check(empty&&*empty=="literal/","terminator encodes empty string");
        auto emptyRange=ResolveStateLiteral(*empty);check(emptyRange.bytes==1&&*static_cast<const char*>(emptyRange.address)==0,"empty string resolves");
        check(!encode(ascii+sizeof(ascii)),"one-past image boundary not read");
        check(!encode(nullptr),"null not treated as string");
        ranges[0].bytes=sizeof(ascii)-1;check(!encode(ascii),"terminator beyond registered range rejected");
        std::array<char,32> unterminated;unterminated.fill('a');ranges[0]={"raw",unterminated.data(),unterminated.size(),false};
        check(!encode(unterminated.data()),"unterminated bounded region rejected");
        std::string maximum(stateliterals::MaxBytes,'x');ranges[0]={"max",maximum.c_str(),maximum.size()+1,false};
        auto maxName=encode(maximum.c_str());check(maxName&&maxName->size()==4096,"maximum encodes within archive identifier bound");
        maximum.push_back('x');ranges[0]={"oversized",maximum.c_str(),maximum.size()+1,false};
        check(!encode(maximum.c_str()),"oversized source rejected");
        rejects("literal/0","odd hex rejected");rejects("literal/gg","non-hex rejected");
        rejects("literal/4D","noncanonical uppercase rejected");rejects("literal/610062","embedded NUL rejected");
        rejects("literal/"+std::string(2*(stateliterals::MaxBytes+1),'1'),"oversized identifier rejected");
        check(!ResolveStateLiteral("other/symbol").address,"nonliteral delegated to another resolver");
        const auto again=ResolveStateLiteral(*name);check(again.address==first.address,"same identifier preserves interned pointer identity");
        const auto savedAgain=EncodeStateLiteral(first.address,{});check(savedAgain&&*savedAgain==*name,"save after load recognizes explicit intern pool ownership");
        const auto internSuffix=EncodeStateLiteral(static_cast<const char*>(first.address)+7,{});
        check(internSuffix&&*internSuffix==*suffix,"interned suffix resaves");
        check(!std::memcmp(first.address,ascii,sizeof(ascii)),"later intern operations preserve prior pointer contents");
        const char resource[]="__OTR__objects/test/mesh";
        ranges[0]={"resource",resource,sizeof(resource),false};
        unsigned lookups=0;
        const auto assetExists=[&](std::string_view path){++lookups;return path=="objects/test/mesh";};
        auto handle=EncodeStateResourceLiteral(resource,ranges,assetExists);
        check(handle&&*handle==*EncodeStateLiteral(resource,ranges)&&lookups==1,"explicit engine resource handle resolves mounted path");
        check(!EncodeStateResourceLiteral(resource,ranges,[](std::string_view){return false;}),"missing mounted resource rejected");
        ranges[0].bytes=6;lookups=0;
        check(!EncodeStateResourceLiteral(resource,ranges,assetExists)&&lookups==0,"short signature range never read or queried");
        ranges[0].bytes=sizeof(resource)-1;
        check(!EncodeStateResourceLiteral(resource,ranges,assetExists)&&lookups==0,"unterminated resource never queried");
        ranges[0]={"ascii",ascii,sizeof(ascii),false};
        check(!EncodeStateResourceLiteral(ascii,ranges,assetExists)&&lookups==0,"ordinary untyped text is not a resource");
        const char emptyResource[]="__OTR__";ranges[0]={"empty-resource",emptyResource,sizeof(emptyResource),false};
        check(!EncodeStateResourceLiteral(emptyResource,ranges,assetExists)&&lookups==0,"empty resource path rejected");
        ranges[0]={"resource",resource,sizeof(resource),false};
        check(!EncodeStateResourceLiteral(resource+sizeof(resource),ranges,assetExists)&&lookups==0,"one-past resource address rejected");
        check(!EncodeStateResourceLiteral(resource,{},assetExists)&&lookups==0,"unregistered resource memory rejected");
        std::cout<<"cases="<<cases<<" failures=0\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
