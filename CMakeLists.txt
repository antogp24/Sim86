cmake_minimum_required(VERSION 3.30)
project(Sim86)

set(CMAKE_CXX_STANDARD 20)

add_executable(Sim86 src/main.cpp
        src/string_builder.cpp
        src/string_builder.h
        src/decoder.cpp
        src/util.h
        src/decoder.h
        src/common_instructions.cpp
        src/common_instructions.h
        src/ascii_colors.h
        src/mov.cpp
        src/mov.h
        src/jumps.h
        src/jumps.cpp)
