#!/bin/bash

ROOT=$(realpath ../../qemu)
BUILD=$(realpath $ROOT/build)
PANDARE_AUTOGEN_DIR=$(realpath ../../include)

LIBPANDAS=$(find $BUILD -maxdepth 1 -name "libpanda-*.so")

for LIBPANDA in $LIBPANDAS; do
    echo "Running $LIBPANDA"
    bash ./gdb_run.sh $LIBPANDA
done

mv _pandare_ffi_*.py ../../python/core/pandare2/autogen
mv panda_*.h ../../include/autogen