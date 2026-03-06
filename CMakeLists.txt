cmake_minimum_required(VERSION 4.1)
project(interpreter C)

set(CMAKE_C_STANDARD 11)

add_executable(interpreter src/main.c
        src/chunk/chunk.c
        src/misc/memory.c
        src/debug/debug.c
        src/misc/value.c
        src/vm/vm.c
        src/compiler/compiler.c
        src/scanner/scanner.c
        src/misc/object.c
        src/table/table.c
)
