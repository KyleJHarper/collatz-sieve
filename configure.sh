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
  mkdir build
  cd build
  cmake -DCMAKE_CXX_STANDARD=20 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release ..
  make
  sudo make install
fi

