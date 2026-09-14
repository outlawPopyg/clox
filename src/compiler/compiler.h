#ifndef INTERPRETER_COMPILER_H
#define INTERPRETER_COMPILER_H

#include "../vm/vm.h"
#include "../misc/object.h"

ObjFunction* compile(const char *source);

#endif //INTERPRETER_COMPILER_H
