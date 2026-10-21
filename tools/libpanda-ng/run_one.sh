#!/bin/bash
if [ "$#" -ne 2 ]; then
   echo "run.sh [QEMU PATH] [libpanda*.so]"
   exit 1
fi

ROOT=$1
LIBPANDA=$2
LOCAL_DIR=$(dirname "$(realpath $0)")
BUILD_PY=$(realpath $LOCAL_DIR/build.py)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd ) 

gdb $LIBPANDA -ex "source $BUILD_PY" -ex "extract_types $ROOT $LIBPANDA $SCRIPT_DIR" -ex "q"

if [ $? -ne 100 ]; then
    echo "Failed to extract types for $LIBPANDA"
    exit 1
else
   echo "Successfully extracted types"
   exit 0
fi