#pragma once

#ifdef __cplusplus

#include <any>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <cassert>
#include <limits>
#include <stdint.h>
#include <unordered_map>

/*
 * This class can attach additional data to pointers. It can only attach a single instance of each type of data.
 * Use the ObjectExtension::Register class to register a type to be used as an object extension.
 * An example usage is:
 *
 * struct MyData {
 *     s32 data = -1;
 * };
 * static ObjectExtension::Register<MyData> MyDataRegister;
 *
 * Then you can get with
 * ObjectExtension::GetInstance().Get<MyData>(ptr);
 * and set with
 * ObjectExtension::GetInstance().Set<MyData>(ptr, MyData{});
 * (or with the returned pointer from Get()).
 */
class ObjectExtension {
  public:
    using Id = uint32_t;

    static constexpr Id InvalidId = std::numeric_limits<Id>::max();

    // Registers type T to be used as an object extension
    template <typename T> class Register {
      public:
        // A non-null name explicitly opts an audited pointer-free POD type into
        // exact states. Nontrivial types need their own semantic serializer.
        Register(const char* stateName = nullptr) {
            auto& extensions=ObjectExtension::GetInstance();
            Id = extensions.RegisterId();
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
            if(stateName) {
                if constexpr(std::is_trivially_copyable_v<T>) {
                    extensions.StateCodecs.emplace(Id, StateCodec{stateName,
                        [](const std::any& value) { const auto& typed=std::any_cast<const T&>(value);
                            std::vector<uint8_t> bytes(sizeof(T));std::memcpy(bytes.data(),&typed,sizeof(T));return bytes; },
                        [](const std::vector<uint8_t>& bytes)->std::any {
                            if(bytes.size()!=sizeof(T))throw std::runtime_error("Extension layout mismatch");
                            T typed{};std::memcpy(&typed,bytes.data(),sizeof(T));return typed; }});
                } else throw std::runtime_error("Nontrivial extension needs an explicit state codec");
            }
#endif
        }

        Register(const char* stateName, std::vector<uint8_t> (*encode)(const T&),
                 T (*decode)(const std::vector<uint8_t>&)) {
            auto& extensions=ObjectExtension::GetInstance();Id=extensions.RegisterId();
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
            extensions.StateCodecs.emplace(Id, StateCodec{stateName,
                [encode](const std::any& value){return encode(std::any_cast<const T&>(value));},
                [decode](const std::vector<uint8_t>& bytes)->std::any{return decode(bytes);}});
#endif
        }

        static ObjectExtension::Id Id;
    };

    // Gets the singleton ObjectExtension instance
    static ObjectExtension& GetInstance();

    // Gets the data of type T associated with an object, or nullptr if no such data has been attached
    template <typename T> T* Get(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object == nullptr) {
            return nullptr;
        }

        auto it = Data.find(std::make_pair(object, ObjectExtension::Register<T>::Id));
        if (it == Data.end()) {
            return nullptr;
        }

        return std::any_cast<T>(&(it->second));
    }

    // Sets the data of type T for an object. Data will be copied.
    template <typename T> void Set(const void* object, const T&& data) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object != nullptr) {
            Data[std::make_pair(object, ObjectExtension::Register<T>::Id)] = data;
        }
    }

    // Returns true if an object has data of type T associated with it
    template <typename T> bool Has(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);
        if (object == nullptr) {
            return false;
        }

        return Data.contains(std::make_pair(object, ObjectExtension::Register<T>::Id));
    }

    // Removes data of type T from an object
    template <typename T> void Remove(const void* object) {
        assert(ObjectExtension::Register<T>::Id != InvalidId);

        Data.erase(std::make_pair(object, ObjectExtension::Register<T>::Id));
    }

    // Removes all data from an object
    void Free(const void* object);

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    struct StateEntry {const void* object;std::string type;std::vector<uint8_t> bytes;};
    std::vector<StateEntry> CaptureState() const {
        std::vector<StateEntry> result;result.reserve(Data.size());
        for(const auto& [key,value]:Data) {
            auto codec=StateCodecs.find(key.second);
            if(codec==StateCodecs.end())throw std::runtime_error(std::string("Unsupported state extension: ")+value.type().name());
            result.push_back({key.first,codec->second.name,codec->second.encode(value)});
        }
        return result;
    }
    std::unique_ptr<ObjectExtension> PrepareState(const std::vector<StateEntry>& entries) const {
        auto result=std::unique_ptr<ObjectExtension>(new ObjectExtension);
        for(const auto& entry:entries) {
            bool found=false;
            for(const auto& [id,codec]:StateCodecs)if(codec.name==entry.type) {
                if(found)throw std::runtime_error("Ambiguous extension state codec");
                if(!entry.object||!result->Data.emplace(std::make_pair(entry.object,id),codec.decode(entry.bytes)).second)
                    throw std::runtime_error("Duplicate extension state owner");
                found=true;
            }
            if(!found)throw std::runtime_error("Unknown extension state type");
        }
        return result;
    }
    void CommitState(ObjectExtension& prepared) noexcept {Data.swap(prepared.Data);}
#endif
  private:
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    struct StateCodec {
        std::string name;
        std::function<std::vector<uint8_t>(const std::any&)> encode;
        std::function<std::any(const std::vector<uint8_t>&)> decode;
    };
    std::unordered_map<Id,StateCodec> StateCodecs;
#endif
    ObjectExtension() = default;

    // Returns the next free object extension Id
    Id RegisterId();

    ObjectExtension::Id NextId = 0;

    struct KeyHash {
        std::size_t operator()(const std::pair<const void*, ObjectExtension::Id>& key) const {
            return std::hash<const void*>{}(key.first) ^ (std::hash<ObjectExtension::Id>{}(key.second) << 1);
        }
    };

    // Collection of all object extension data.
    std::unordered_map<std::pair<const void*, ObjectExtension::Id>, std::any, KeyHash> Data;
};

// Static template globals
template <typename T> ObjectExtension::Id ObjectExtension::Register<T>::Id = ObjectExtension::InvalidId;

extern "C" {
#endif // __cplusplus

void ObjectExtension_Free(const void* object);
#ifdef __cplusplus
}
#endif