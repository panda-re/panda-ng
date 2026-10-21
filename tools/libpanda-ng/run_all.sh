#!/bin/bash

if [[ $? -ne 1 ]]; then
    echo "Usage: run_all.sh [QEMU_PATH]"
fi

ROOT=$1
BUILD=$ROOT/build
LIBPANDAS=$(find $BUILD -maxdepth 1 -name "libpanda-*.so")

LOCAL_DIR=$(dirname "$(realpath $0)")
RUN_ONE=$LOCAL_DIR/run_one.sh


for LIBPANDA in $LIBPANDAS; do
    echo "Running $LIBPANDA"
    bash $RUN_ONE $ROOT $LIBPANDA
    if [[ $? -ne 0 ]]; then
        echo "Terminating run_all.sh due to failure in $LIBPANDA"
        exit 1
    fi
done