cmake_minimum_required(VERSION 4.1)
project(interpreter C)

set(CMAKE_C_STANDARD 11)

add_executable(interpreter src/main.c
        src/chunk.c
        src/memory.c
        src/debug.c
        src/value.c
        src/vm.c)
