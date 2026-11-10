#ifndef INTERPRETER_OBJECT_H
#define INTERPRETER_OBJECT_H

#include "common.h"
#include "value.h"
#include "../chunk/chunk.h"
#include "../table/table.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_STRING(value)    isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value)   isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value)     isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value)  isObjType(value, OBJ_INSTANCE)

#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)    (((ObjNative*)AS_OBJ(value))->function)
#define AS_CLOSURE(value)   ((ObjClosure*)AS_OBJ(value))
#define AS_CLASS(value)     ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))

typedef enum {
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLASS,
    OBJ_INSTANCE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

ObjString* copyString(const char* chars, int length);
void printObject(Value value);
ObjString* takeString(char* chars, int length);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);

static bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif //INTERPRETER_OBJECT_H