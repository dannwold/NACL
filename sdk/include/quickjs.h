#ifndef QUICKJS_H
#ifndef QUICKJS_H_NACL
#define QUICKJS_H_NACL

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JSRuntime JSRuntime;
typedef struct JSContext JSContext;
typedef struct JSObject JSObject;
typedef struct JSClass JSClass;
typedef uint32_t JSClassID;
typedef struct JSModuleDef JSModuleDef;

typedef struct JSValue {
    union {
        int32_t int32;
        double float64;
        void *ptr;
    } u;
    int64_t tag;
} JSValue;

typedef JSValue JSValueConst;

#define JS_TAG_FIRST       -11
#define JS_TAG_BIG_INT     -10
#define JS_TAG_SYMBOL      -8
#define JS_TAG_STRING      -7
#define JS_TAG_SHAPE       -6
#define JS_TAG_ASYNC_FUNCTION -5
#define JS_TAG_VAR_REF     -4
#define JS_TAG_MODULE      -3
#define JS_TAG_FUNCTION_BYTECODE -2
#define JS_TAG_OBJECT      -1
#define JS_TAG_INT          0
#define JS_TAG_BOOL         1
#define JS_TAG_NULL         2
#define JS_TAG_UNDEFINED    3
#define JS_TAG_UNINITIALIZED 4
#define JS_TAG_CATCH        5
#define JS_TAG_EXCEPTION    6
#define JS_TAG_FLOAT64      7

#define JS_NULL (JSValue){ .u.ptr = NULL, .tag = JS_TAG_NULL }
#define JS_UNDEFINED (JSValue){ .u.ptr = NULL, .tag = JS_TAG_UNDEFINED }
#define JS_FALSE (JSValue){ .u.int32 = 0, .tag = JS_TAG_BOOL }
#define JS_TRUE (JSValue){ .u.int32 = 1, .tag = JS_TAG_BOOL }
#define JS_EXCEPTION (JSValue){ .u.ptr = NULL, .tag = JS_TAG_EXCEPTION }

typedef void JSClassFinalizer(JSRuntime *rt, JSValue val);

typedef struct JSClassDef {
    const char *class_name;
    JSClassFinalizer *finalizer;
    void (*gc_mark)(JSRuntime *rt, JSValueConst val, void (*mark_func)(JSRuntime *rt, JSValueConst val));
    int (*call)(JSContext *ctx, JSValueConst func_obj, JSValueConst this_val, int argc, JSValueConst *argv, int flags);
} JSClassDef;

typedef JSValue JSCFunction(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
typedef JSValue JSCFunctionMagic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
typedef JSValue JSCFunctionData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *data);

typedef struct JSCFunctionListEntry {
    const char *name;
    uint8_t prop_flags;
    uint8_t def_type;
    int16_t magic;
    union {
        struct {
            uint8_t length;
            uint8_t cproto;
            JSCFunction *cfunc;
        } func;
        const char *str;
        int32_t i32;
        int64_t i64;
        double f64;
    } u;
} JSCFunctionListEntry;

#define JS_CFUNC_DEF(name, length, func1) { name, 0, 0, 0, .u.func = { length, 0, (JSCFunction *)func1 } }

// Function Declarations
void JS_FreeValue(JSContext *ctx, JSValue v);
void JS_FreeCString(JSContext *ctx, const char *ptr);
const char *JS_ToCString(JSContext *ctx, JSValueConst val);
const char *JS_ToCStringLen(JSContext *ctx, size_t *plen, JSValueConst val1);
int JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val);
int JS_ToUint64(JSContext *ctx, uint64_t *pres, JSValueConst val);

JSValue JS_NewString(JSContext *ctx, const char *str);
JSValue JS_NewStringLen(JSContext *ctx, const char *str1, size_t len1);
JSValue JS_NewInt32(JSContext *ctx, int32_t val);
JSValue JS_NewInt64(JSContext *ctx, int64_t val);
JSValue JS_NewBigInt64(JSContext *ctx, int64_t val);
JSValue JS_NewFloat64(JSContext *ctx, double d);
JSValue JS_NewBool(JSContext *ctx, int val);

JSValue JS_NewObject(JSContext *ctx);
JSValue JS_NewObjectClass(JSContext *ctx, JSClassID class_id);
JSValue JS_NewArray(JSContext *ctx);
JSValue JS_NewArrayBuffer(JSContext *ctx, uint8_t *buf, size_t size, void (*free_func)(JSRuntime *rt, void *opaque, void *ptr), void *opaque, int is_shared);
uint8_t *JS_GetArrayBuffer(JSContext *ctx, size_t *pbyte_offset, size_t *pbyte_length, JSValueConst obj);

JSValue JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);
JSValue JS_ThrowRangeError(JSContext *ctx, const char *fmt, ...);
JSValue JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...);

int JS_IsException(JSValueConst v);
int JS_IsUndefined(JSValueConst v);
int JS_IsString(JSValueConst v);
int JS_IsFunction(JSContext *ctx, JSValueConst v);

JSClassID JS_NewClassID(JSClassID *pclass_id);
int JS_NewClass(JSRuntime *rt, JSClassID class_id, const JSClassDef *class_def);
JSRuntime *JS_GetRuntime(JSContext *ctx);

void JS_SetClassProto(JSContext *ctx, JSClassID class_id, JSValue obj);
void JS_SetOpaque(JSValue obj, void *opaque);
void *JS_GetOpaque(JSValueConst obj, JSClassID class_id);
void *JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id);

int JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj, const char *prop, JSValue val);
int JS_SetPropertyUint32(JSContext *ctx, JSValueConst this_obj, uint32_t idx, JSValue val);
void JS_SetPropertyFunctionList(JSContext *ctx, JSValueConst obj, const JSCFunctionListEntry *tab, int len);

typedef int JSModuleInitFunc(JSContext *ctx, JSModuleDef *m);
JSModuleDef *JS_NewCModule(JSContext *ctx, const char *name_str, JSModuleInitFunc *func);
int JS_AddModuleExport(JSContext *ctx, JSModuleDef *m, const char *name_str);
int JS_AddModuleExportList(JSContext *ctx, JSModuleDef *m, const JSCFunctionListEntry *tab, int len);
int JS_SetModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name, JSValue val);
int JS_SetModuleExportList(JSContext *ctx, JSModuleDef *m, const JSCFunctionListEntry *tab, int len);

JSValue JS_GetGlobalObject(JSContext *ctx);
JSValue JS_Call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv);
JSValue JS_DupValue(JSContext *ctx, JSValueConst v);
JSValue JS_CFUNC_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue JS_NewCFunction2(JSContext *ctx, JSCFunction *func, const char *name, int length, int cproto, int magic);

#ifdef __cplusplus
}
#endif

#endif // QUICKJS_H_NACL
#endif // QUICKJS_H
