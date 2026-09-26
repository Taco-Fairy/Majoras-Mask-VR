#pragma once
#include <algorithm>
#include <set>
#include <string>
#include <vector>
namespace mmvr {
struct ModPack { std::string id, name, path; bool enabled = true; };
inline std::vector<ModPack> modPacks;
inline void (*refreshMods)() = nullptr;
inline void (*toggleMod)(int) = nullptr;
struct ModFolder { std::string key, label; int depth; std::vector<int> children; };
inline std::vector<ModFolder> modFolders;
inline std::vector<int> modPackFolders;
// Negative IDs describe folders; nonnegative IDs describe actual pack indices.
inline int FolderEntry(int index) { return -2-index; }
inline int FolderIndex(int entry) { return -2-entry; }
inline bool ModFolderRow(int row) { return row <= -2 && FolderIndex(row) < int(modFolders.size()); }
inline std::string ModDisplayPath(std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    if (name.rfind("mods/",0)!=0 && name.rfind("texturepacks/",0)!=0) name="mods/"+name;
    return name;
}
inline void RebuildModFolders() {
    modFolders = {{"mods","Mods",0,{}},{"texturepacks","Texture Packs",0,{}}};
    modPackFolders.assign(modPacks.size(),0);
    for (size_t pack=0; pack<modPacks.size(); ++pack) {
        auto path=ModDisplayPath(modPacks[pack].name);
        int parent=path.rfind("texturepacks/",0)==0?1:0;
        size_t at=path.find('/')+1;
        for (auto end=path.find('/',at); end!=std::string::npos; end=path.find('/',at)) {
            auto part=path.substr(at,end-at);at=end+1;
            if(part.empty() || part=="." || part=="..") continue;
            const auto key=modFolders[parent].key+"/"+part;
            auto found=std::find_if(modFolders.begin(),modFolders.end(),[&](const auto& f){return f.key==key;});
            int child=int(found-modFolders.begin());
            if(found==modFolders.end()) {
                const int depth=modFolders[parent].depth+1;
                modFolders.push_back({key,part,depth,{}});
                modFolders[parent].children.push_back(FolderEntry(child));
            }
            parent=child;
        }
        modPackFolders[pack]=parent;
        modFolders[parent].children.push_back(int(pack));
    }
}
inline std::vector<int> VisibleModEntries(const std::set<std::string>& expanded) {
    std::vector<int> result;
    auto visit=[&](auto&& self,int folder)->void {
        result.push_back(FolderEntry(folder));
        if (!expanded.contains(modFolders[folder].key)) return;
        for (int child:modFolders[folder].children) {
            if(child<0) self(self,FolderIndex(child)); else result.push_back(child);
        }
    };
    if (modFolders.size()>=2) { visit(visit,0);visit(visit,1); }
    return result;
}
inline std::string ModPackLabel(int index) {
    const auto name=ModDisplayPath(modPacks[index].name);
    return name.substr(name.find_last_of('/')+1);
}
}
