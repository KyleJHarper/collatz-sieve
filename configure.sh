#!/bin/bash


BASE_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
INCLUDE_DIR="${BASE_DIR}/src/include"

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
# CMake
#
[ -d build ] || mkdir build
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
touch build/Release/compile_commands.json
[ -e compile_commands.json ] || ln -s build/Release/compile_commands.json compile_commands.json

