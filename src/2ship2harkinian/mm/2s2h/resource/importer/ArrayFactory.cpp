#include "2s2h/resource/importer/ArrayFactory.h"
#include "2s2h/resource/type/Array.h"
#include <fast/lus_gbi.h>

namespace SOH {

static constexpr const char* legacyMountainFragments[] = {
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_000D70",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_000E30",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_000EF0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_000FB8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001078",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001138",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001200",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0012C0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001380",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001448",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001508",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0015C8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001690",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001750",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001810",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0018D8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001998",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001A58",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001B20",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001BE0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001CA0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001D68",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001E28",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001EE8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_001FB0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002070",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002130",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0021F0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0022B0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002370",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002430",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0024F0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0025B0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002670",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002730",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0027F0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0028B0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002970",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002A30",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002AF0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002BB0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002C70",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002D30",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002DF0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002EB0",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_002F78",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_003038",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0030F8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0031B8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_003278",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_003338",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0033F8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_0034B8",
        "__OTR__objects/object_hanareyama_obj/object_hanareyama_obj_DL_003578",
};

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryArrayV0::ReadResource(std::shared_ptr<Ship::File> file,
                                           std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto array = std::make_shared<Array>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    array->ArrayType = (ArrayResourceType)reader->ReadUInt32();
    array->ArrayCount = reader->ReadUInt32();

    if (array->ArrayType == ArrayResourceType::Pointer) {
        if (array->ArrayCount > 65536) return nullptr;
        if (initData->ResourceVersion == 1) {
            array->ResourcePaths.reserve(array->ArrayCount);
            for (uint32_t i = 0; i < array->ArrayCount; ++i) {
                std::string path = reader->ReadString();
                array->ResourcePaths.push_back(path.empty() ? "" : "__OTR__" + path);
            }
            for (const auto& path : array->ResourcePaths) array->Pointers.push_back(path.empty() ? nullptr : path.c_str());
        } else if (initData->Path == "objects/object_hanareyama_obj/object_hanareyama_obj_DLArray_004638" && array->ArrayCount == 54) {
            // Older archives contain host-memory bytes here, not ROM pointers.
            array->Pointers.assign(std::begin(legacyMountainFragments), std::end(legacyMountainFragments));
        } else {
            return nullptr;
        }
        return array;
    }
    if (initData->ResourceVersion != 0) return nullptr;
    for (uint32_t i = 0; i < array->ArrayCount; i++) {
        if (array->ArrayType == ArrayResourceType::Vertex) {
            // OTRTODO: Implement Vertex arrays as just a vertex resource.
            Fast::F3DVtx data;
            data.v.ob[0] = reader->ReadInt16();
            data.v.ob[1] = reader->ReadInt16();
            data.v.ob[2] = reader->ReadInt16();
            data.v.flag = reader->ReadUInt16();
            data.v.tc[0] = reader->ReadInt16();
            data.v.tc[1] = reader->ReadInt16();
            data.v.cn[0] = reader->ReadUByte();
            data.v.cn[1] = reader->ReadUByte();
            data.v.cn[2] = reader->ReadUByte();
            data.v.cn[3] = reader->ReadUByte();
            array->Vertices.push_back(data);
        } else {
            array->ArrayScalarType = (ScalarType)reader->ReadUInt32();

            int iter = 1;

            if (array->ArrayType == ArrayResourceType::Vector) {
                iter = reader->ReadUInt32();
            }

            for (int k = 0; k < iter; k++) {
                ScalarData data{};

                switch (array->ArrayScalarType) {
                    case ScalarType::ZSCALAR_S8:
                        data.s8 = reader->ReadInt8();
                        break;
                    case ScalarType::ZSCALAR_U8:
                    case ScalarType::ZSCALAR_X8:
                        data.u8 = reader->ReadUByte();
                        break;
                    case ScalarType::ZSCALAR_S16:
                        data.s16 = reader->ReadInt16();
                        break;
                    case ScalarType::ZSCALAR_U16:
                    case ScalarType::ZSCALAR_X16:
                        data.u16 = reader->ReadUInt16();
                        break;
                    case ScalarType::ZSCALAR_S32:
                        data.s32 = reader->ReadInt32();
                        break;
                    case ScalarType::ZSCALAR_U32:
                    case ScalarType::ZSCALAR_X32:
                        data.u32 = reader->ReadUInt32();
                        break;
                    case ScalarType::ZSCALAR_S64:
                        data.s64 = reader->ReadInt64();
                        break;
                    case ScalarType::ZSCALAR_U64:
                    case ScalarType::ZSCALAR_X64:
                        data.u64 = reader->ReadUInt64();
                        break;
                    default:
                        // OTRTODO: IMPLEMENT OTHER TYPES!
                        break;
                }

                array->Scalars.push_back(data);
            }
        }
    }

    return array;
}
} // namespace SOH
