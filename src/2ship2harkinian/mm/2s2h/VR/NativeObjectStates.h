#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeInteractionStates.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
extern "C" {
#include "buffers.h"
}
namespace mmvrgame {
inline constexpr const char* ObjectStateId="engine/object-extensions";
inline mmvr::states::Component ObjectStateComponent() {
    using namespace mmvr::states;using nlohmann::json;
    struct Prepared final:PreparedComponent {
        std::unique_ptr<ObjectExtension> data;
        void Commit() noexcept override {if(data){ObjectExtension::GetInstance().CommitState(*data);data.reset();}}
    };
    return {ObjectStateId,1,[] {
        auto list=json::array();const auto heap=reinterpret_cast<uintptr_t>(gSystemHeap);
        for(const auto& value:ObjectExtension::GetInstance().CaptureState()) {
            const auto address=reinterpret_cast<uintptr_t>(value.object);
            if(address<heap||address-heap>=SYSTEM_HEAP_SIZE)throw Error("Extension owner is outside native world storage");
            list.push_back({{"owner",address-heap},{"type",value.type},{"bytes",value.bytes}});
        }
        std::sort(list.begin(),list.end(),[](const auto& a,const auto& b){
            return std::pair{a.at("owner").template get<uint64_t>(),a.at("type").template get<std::string>()}<
                   std::pair{b.at("owner").template get<uint64_t>(),b.at("type").template get<std::string>()};});
        return stateinteraction::Encode(ObjectStateId,list);
    },[](const Block& block)->std::unique_ptr<PreparedComponent> {
        auto list=stateinteraction::Decode(block,ObjectStateId);
        if(!list.is_array()||list.size()>32768)throw Error("Invalid extension state count");
        std::vector<ObjectExtension::StateEntry> entries;
        for(const auto& value:list) {
            const auto offset=stateinteraction::Integer<uint32_t>(value,"owner");
            if(offset>=SYSTEM_HEAP_SIZE)throw Error("Extension owner exceeds native world storage");
            auto type=value.at("type").get<std::string>();
            if(type.empty()||type.size()>128)throw Error("Invalid extension state type");
            const auto& bytes=value.at("bytes");
            if(!bytes.is_array()||bytes.size()>4096)throw Error("Invalid extension payload size");
            std::vector<uint8_t> payload;payload.reserve(bytes.size());
            for(const auto& byte:bytes) {
                if(!byte.is_number_unsigned()||byte.get<uint64_t>()>255)throw Error("Invalid extension byte");
                payload.push_back(byte.get<uint8_t>());
            }
            entries.push_back({gSystemHeap+offset,std::move(type),std::move(payload)});
        }
        auto prepared=std::make_unique<Prepared>();
        prepared->data=ObjectExtension::GetInstance().PrepareState(entries);return prepared;
    }};
}
}
#endif
