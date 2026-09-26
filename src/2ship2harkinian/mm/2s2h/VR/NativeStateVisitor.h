#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// C-compatible adapter for compiler-generated visitors. The visitor runs only
// at a quiescent save boundary. A pointer callback describes a field address,
// not the pointed-to memory; unknown ownership is rejected by the backend.
struct MMVR_StateSink;
typedef void (*MMVR_StateActorVisitor)(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderJntSphElement(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderTrisElement(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderQuad(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderCylinder(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderSphere(struct MMVR_StateSink*, void*);
void MMVR_StateVisitColliderTris(struct MMVR_StateSink*, void*);
void MMVR_StateVisitAudioCmd(struct MMVR_StateSink*, const void*, const char*);
int MMVR_StateCameraVariant(const void* camera);
int MMVR_StateCameraDoorVariant(const void* camera);

typedef struct MMVR_StateSink {
    void* context;
    void (*actor)(void*, int id, size_t bytes, MMVR_StateActorVisitor);
    void (*block)(void*, const char* id, void* address, size_t bytes);
    void (*constant)(void*, const char* id, const void* address, size_t bytes);
    void (*function)(void*, const char* id, void (*address)(void));
    // kind: 0 data, 1 function, 2 plain-char pointer (possible immutable text).
    void (*pointer)(void*, const void* field, int kind, const char* name);
    int (*variant)(void*, const void* root, const void* storage, size_t bytes, const char* path, size_t count);
    void (*unsupported)(void*, const char* path);
    void (*array)(void*, const void* address, int count, size_t elementBytes, MMVR_StateActorVisitor, const char* path);
    // Optional cross-build ABI contract, evaluated by the actual target compiler.
    void (*layout)(void*, const char* id, const char* type, size_t offset, size_t bytes, size_t alignment, size_t count);
    void (*layoutBytes)(void*, const char* id, const void* bytes, size_t count);
    void (*layoutUnsupported)(void*, const char* id, const char* reason);
} MMVR_StateSink;
#ifdef _MSC_VER
#define MMVR_STATE_ALIGN_TYPE(type) __alignof(type)
#else
#define MMVR_STATE_ALIGN_TYPE(type) _Alignof(type)
#endif
// C11 selects the expression's actual target type, not libclang's Android type.
#define MMVR_STATE_SCALAR_ALIGN(value) _Generic((value), \
    _Bool: MMVR_STATE_ALIGN_TYPE(_Bool), char: MMVR_STATE_ALIGN_TYPE(char), \
    signed char: MMVR_STATE_ALIGN_TYPE(signed char), unsigned char: MMVR_STATE_ALIGN_TYPE(unsigned char), \
    short: MMVR_STATE_ALIGN_TYPE(short), unsigned short: MMVR_STATE_ALIGN_TYPE(unsigned short), \
    int: MMVR_STATE_ALIGN_TYPE(int), unsigned int: MMVR_STATE_ALIGN_TYPE(unsigned int), \
    long: MMVR_STATE_ALIGN_TYPE(long), unsigned long: MMVR_STATE_ALIGN_TYPE(unsigned long), \
    long long: MMVR_STATE_ALIGN_TYPE(long long), unsigned long long: MMVR_STATE_ALIGN_TYPE(unsigned long long), \
    float: MMVR_STATE_ALIGN_TYPE(float), double: MMVR_STATE_ALIGN_TYPE(double), \
    long double: MMVR_STATE_ALIGN_TYPE(long double), default: 0)
#ifdef __cplusplus
}
#endif
