#ifndef INTERPRETER_VM_H
#define INTERPRETER_VM_H

#include "../chunk/chunk.h"
#include "../misc/value.h"
#include "../table/table.h"
#include "../misc/object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX]; // память для массива выделяется автоматически
    Value* stackTop;
    Obj* objects;
    Table strings;
    Table globals;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif //INTERPRETER_VM_H