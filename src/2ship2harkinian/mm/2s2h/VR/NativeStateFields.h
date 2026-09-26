#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include <type_traits>
namespace mmvrgame {
// Only explicitly listed, trivially copyable gameplay fields may join the
// native archive. Container internals, resources and device objects are excluded.
template<class T> void NativeStateField(MMVR_StateSink* sink,const char* id,T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    sink->block(sink->context,id,&value,sizeof(value));
    if constexpr(std::is_pointer_v<T>)
        sink->pointer(sink->context,&value,std::is_function_v<std::remove_pointer_t<T>>,id);
}
}
#endif
