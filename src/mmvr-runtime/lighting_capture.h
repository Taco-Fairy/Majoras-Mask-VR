#pragma once
#include <cstdint>
namespace mmvr {
#pragma pack(push,1)
struct LightingCaptureHeader {
    char magic[8]={'M','M','V','L','C','A','P','1'};
    uint32_t version=1;
    uint32_t recordBytes=1800;
};
struct LightingCaptureRecord {
    uint32_t pose=0,batch=0,count=0,dest=0,mode=0,lightCount=0;
    uint16_t scaleS=0,scaleT=0;
    int16_t fogMul=0,fogOffset=0;
    float model[16]{},mp[16]{};
    uint8_t lights[33*16]{};
    uint8_t lookat[2*12]{};
    uint8_t vertices[68*16]{};
};
#pragma pack(pop)
static_assert(sizeof(LightingCaptureHeader)==16);
static_assert(sizeof(LightingCaptureRecord)==1800);
bool LightingCaptureActive() noexcept;
void BeginLightingCapture(const char* label);
void EndLightingCapture() noexcept;
void WriteLightingCapture(const LightingCaptureRecord& record) noexcept;
}
