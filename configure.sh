#!/bin/bash


BASE_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"


#
# External Repos
#
EXTERNAL_REPOS_DIR="${BASE_DIR}/src/include/external_repos"

# Spdlog
cd "${EXTERNAL_REPOS_DIR}"
git clone https://github.com/gabime/spdlog.git
cd spdlog
git checkout v1.15.3

# CLI11
cd "${EXTERNAL_REPOS_DIR}"
git clone https://github.com/CLIUtils/CLI11.git
cd CLI11
git checkout v2.5.0


