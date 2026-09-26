#include "ArrayExporter.h"
#include "VtxExporter.h"
#include <ZVector.h>
#include <ZPointer.h>
#include <Globals.h>
#include "DisplayListExporter.h"
#include <stdexcept>
void OTRExporter_Array::Save(ZResource* res, const fs::path& outPath, BinaryWriter* writer)
{
	ZArray* arr = (ZArray*)res;

	const bool pointers = arr->resList.at(0)->GetResourceType() == ZResourceType::Pointer;
    // Version 1 stores resource names for pointers; never cast a ZPointer to ZScalar.
    WriteHeader(res, outPath, writer, static_cast<uint32_t>(SOH::ResourceType::SOH_Array), pointers ? 1 : 0);

	writer->Write((uint32_t)arr->resList[0]->GetResourceType());
	writer->Write((uint32_t)arr->arrayCnt);

	for (size_t i = 0; i < arr->arrayCnt; i++)
	{
		if (pointers)
        {
            auto* ptr = dynamic_cast<ZPointer*>(arr->resList[i]);
            if (!ptr || ptr->type != "Gfx") throw std::runtime_error("Unsupported resource pointer array");
            if (ptr->ptr == 0) { writer->Write(""); continue; }
            std::string name;
            if (!Globals::Instance->GetSegmentedPtrName(ptr->ptr, ptr->parent, "", name, ptr->parent->workerID))
                throw std::runtime_error("Unresolved display-list array pointer");
            if (name.starts_with("&")) name.erase(0,1);
            writer->Write(OTRExporter_DisplayList::GetPathToRes(ptr, name));
        }
        else if (arr->resList[i]->GetResourceType() == ZResourceType::Vertex)
		{
			ZVtx* vtx = (ZVtx*)arr->resList[i];
			writer->Write(vtx->x);
			writer->Write(vtx->y);
			writer->Write(vtx->z);
			writer->Write(vtx->flag);
			writer->Write(vtx->s);
			writer->Write(vtx->t);
			writer->Write(vtx->r);
			writer->Write(vtx->g);
			writer->Write(vtx->b);
			writer->Write(vtx->a);
		}
		else if (arr->resList[i]->GetResourceType() == ZResourceType::Vector)
		{
			ZVector* vec = (ZVector*)arr->resList[i];
			writer->Write((uint32_t)vec->scalarType);
			writer->Write((uint32_t)vec->dimensions);

			for (size_t k = 0; k < vec->dimensions; k++)
			{
				// OTRTODO: Duplicate code here. Cleanup at a later date...
				switch (vec->scalarType)
				{
				case ZScalarType::ZSCALAR_S8:
					writer->Write(vec->scalars[k].scalarData.s8);
					break;
				case ZScalarType::ZSCALAR_U8:
				case ZScalarType::ZSCALAR_X8:
					writer->Write(vec->scalars[k].scalarData.u8);
					break;
				case ZScalarType::ZSCALAR_S16:
					writer->Write(vec->scalars[k].scalarData.s16);
					break;
				case ZScalarType::ZSCALAR_U16:
				case ZScalarType::ZSCALAR_X16:
					writer->Write(vec->scalars[k].scalarData.u16);
					break;
				case ZScalarType::ZSCALAR_S32:
					writer->Write(vec->scalars[k].scalarData.s32);
					break;
				case ZScalarType::ZSCALAR_U32:
				case ZScalarType::ZSCALAR_X32:
					writer->Write(vec->scalars[k].scalarData.u32);
					break;
				case ZScalarType::ZSCALAR_S64:
					writer->Write(vec->scalars[k].scalarData.s64);
					break;
				case ZScalarType::ZSCALAR_U64:
				case ZScalarType::ZSCALAR_X64:
					writer->Write(vec->scalars[k].scalarData.u64);
					break;
					// OTRTODO: ADD OTHER TYPES
				default:
					break;
				}
			}
		}
		else
		{
			ZScalar* scal = (ZScalar*)arr->resList[i];

			writer->Write((uint32_t)scal->scalarType);

			switch (scal->scalarType)
			{
			case ZScalarType::ZSCALAR_S8:
				writer->Write(scal->scalarData.s8);
				break;
			case ZScalarType::ZSCALAR_U8:
			case ZScalarType::ZSCALAR_X8:
				writer->Write(scal->scalarData.u8);
				break;
			case ZScalarType::ZSCALAR_S16:
				writer->Write(scal->scalarData.s16);
				break;
			case ZScalarType::ZSCALAR_U16:
			case ZScalarType::ZSCALAR_X16:
				writer->Write(scal->scalarData.u16);
				break;
			case ZScalarType::ZSCALAR_S32:
				writer->Write(scal->scalarData.s32);
				break;
			case ZScalarType::ZSCALAR_U32:
			case ZScalarType::ZSCALAR_X32:
				writer->Write(scal->scalarData.u32);
				break;
			case ZScalarType::ZSCALAR_S64:
				writer->Write(scal->scalarData.s64);
				break;
			case ZScalarType::ZSCALAR_U64:
			case ZScalarType::ZSCALAR_X64:
				writer->Write(scal->scalarData.u64);
				break;
				// OTRTODO: ADD OTHER TYPES
			default:
				break;
			}
		}
	}
}
