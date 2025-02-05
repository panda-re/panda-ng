#!/bin/bash

targets=(aarch64 arm i386 mips64el mips64 mipsel mips ppc64 ppc x86_64)

for target in "${targets[@]}"; do
  echo "Generating bindings for ${target}..."

  bindgen ../../../include/panda.h \
      --no-prepend-enum-name \
      --blocklist-var IPPORT_RESERVED \
      -o "src/autogen/${target}.rs" \
      -- \
      -D PANDA_TGT=${target} -D __STDC_FORMAT_MACROS \
      $(pkg-config --cflags glib-2.0)
done
