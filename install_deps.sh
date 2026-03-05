#!/bin/bash

# Just a convenience.  Installs packages required for building.  Debian-based only so far.

function fatal {
    echo "${@}" >&2
    exit 1
}



if [ -f /etc/debian_version ] ; then
    [ ${EUID} -eq 0 ] || fatal "You need to be root."
    apt install build-essential cmake libtbb-dev libjemalloc-dev libgmp-dev nvidia-cuda-toolkit
else
    fatal "Not programmed to handle deps on this system type."
fi

