#ifndef INTERPRETER_COMPILER_H
#define INTERPRETER_COMPILER_H

#include "../vm/vm.h"

bool compile(const char *source, Chunk *chunk);

#endif //INTERPRETER_COMPILER_H
