#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "Usage: run_package.sh [QEMU_PATH]"
    exit 1
fi

LOCAL_DIR=$(dirname "$(realpath $0)")
RUN_ONE=$LOCAL_DIR/run_all.sh

bash $RUN_ONE $1
mkdir panda-ng
mv panda_*.h panda-ng

cd $1
TAG=$(git describe --tags)
cd -
echo "Associated QEMU version is $TAG" > panda-ng/version.txt
tar -czvf libpanda-ng.tar.gz panda-ng