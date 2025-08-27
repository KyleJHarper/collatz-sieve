echo "Remember, you should source this, not invoke it." >&2

echo "Setting OMP_NUM_THREADS to 1." >&2
export OMP_NUM_THREADS=1

declare JEMALLOC_PATH='/usr/lib/x86_64-linux-gnu/libjemalloc.so.2'
echo "Setting LD_PRELOAD to use JEMALLOC at: ${JEMALLOC_PATH}" >&2
export LD_PRELOAD="${JEMALLOC_PATH}"

