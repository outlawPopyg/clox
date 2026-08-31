#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc/common.h"
#include "chunk/chunk.h"
#include "compiler/compiler.h"
#include "debug/debug.h"
#include "vm/vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // interpret(line);
    }
}

static char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END); // переместиться в конец файла
    size_t fileSize = ftell(file);
    // считаем сколько байт мы уже считали, т.к сейчас мы в конце файла, то получаем размер файла
    rewind(file); // обратно в начало

    char *buffer = malloc(fileSize + 1); // выдяляем память под строку размером с длину файла
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char *path) {
    char *sourceCode = readFile(path);
    InterpretResult result = interpret(sourceCode);
    free(sourceCode);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
    initVM();
    interpret("var a = \"abc\";"
                    "var b = a + \"cde\";"
                    "a = b;"
                    "print a;");

    freeVM();
    return 0;
}

//0 OP_CONST
//1 loopstart
//1 OP_CONST
//2 OP_ADD
//3 OP_ADD
//4 OP_LOOP
//5 0xff
//6 0xff
//7 OP_CONST


//0 OP_JUMP
//1 0xff
//2 0xff
//3 OP_CONST
//4 OP_ADD
//5 OP_ADD
// patch
//6 OP_CONST
// 00000010 00000000

//0
//1 OP_CONST
//2 OP_ADD
//3 OP_ADD
//4 OP_CONST
//5 OP_JUMP_IF_FALSE
//6 0xff
//7 0xff
//8 OP_POP
// statement
//9 OP_LOOP
//10 11
//11 11