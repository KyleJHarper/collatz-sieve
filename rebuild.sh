#!/bin/bash


# Setup build dir and such.
[ -d build ] || mkdir build
#cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
#cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
touch build/Release/compile_commands.json
[ -e compile_commands.json ] || ln -s build/Release/compile_commands.json compile_commands.json


# Build
#cmake --build build/Debug || exit 1
cmake --build build/Debug --parallel || exit 1
cmake --build build/Release --parallel
