#!/bin/bash

#
# Calls cmake to rebuild anything that needs it.
#
cmake --build build/Debug
cmake --build build/Release

