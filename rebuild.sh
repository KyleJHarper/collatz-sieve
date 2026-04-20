#!/bin/bash

BASE_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
INCLUDE_DIR="${BASE_DIR}/include"

# Make sure the include dir exists.
[ -d "${INCLUDE_DIR}" ] || mkdir "${INCLUDE_DIR}"

#
# External Repos
#
# Spdlog
if [ -d "${INCLUDE_DIR}/spdlog" ] ; then
  echo "Spdlog already cloned.  Ignoring." >&2
else
  cd "${INCLUDE_DIR}"
  git clone https://github.com/gabime/spdlog.git
  cd spdlog
  git checkout v1.15.3
fi
#
# CLI11
if [ -d "${INCLUDE_DIR}/CLI11" ] ; then
  echo "CLI11 already cloned.  Ignoring." >&2
else
  cd "${INCLUDE_DIR}"
  git clone https://github.com/CLIUtils/CLI11.git
  cd CLI11
  git checkout v2.5.0
fi
#
# Abseil CPP
if [ -d "${INCLUDE_DIR}/abseil-cpp" ] ; then
  echo "Abseil-CPP already cloned.  Ignoring." >&2
else
  cd "${INCLUDE_DIR}"
  git clone https://github.com/abseil/abseil-cpp.git
  cd abseil-cpp
  git checkout 20250814.0
fi
#
# CRoaring
if [ -d "${INCLUDE_DIR}/CRoaring" ] ; then
  echo "CRoaring already cloned.  Ignoring." >&2
else
  cd "${INCLUDE_DIR}"
  git clone https://github.com/RoaringBitmap/CRoaring.git
  cd CRoaring
  git checkout v4.6.1
fi
#
# Zstd
if [ -d "${INCLUDE_DIR}/zstd" ] ; then
  echo "Zstd already cloned.  Ignoring." >&2
else
  cd "${INCLUDE_DIR}"
  git clone https://github.com/facebook/zstd.git
  cd zstd
  git checkout v1.5.7
fi


#
# CMake
#
cd ${BASE_DIR}
[ -d build ] || mkdir build
#cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
#cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
touch build/Release/compile_commands.json
[ -e compile_commands.json ] || ln -s build/Release/compile_commands.json compile_commands.json


#
# Calls cmake to rebuild anything that needs it.
#
#cmake --build build/Debug || exit 1
cmake --build build/Debug --parallel || exit 1
cmake --build build/Release --parallel

