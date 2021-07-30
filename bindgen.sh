#!/bin/bash

bindgen --opaque-type=FILE \
    --no-prepend-enum-name \
    --allowlist-function 'grk_.*' \
    --blocklist-type 'FILE' \
    --blocklist-type '_IO_FILE' \
    --raw-line 'use libc::FILE as FILE;' \
    --size_t-is-usize \
    --distrust-clang-mangling \
    --no-layout-tests \
    -o src/lib.rs \
    ./external/grok/src/lib/jp2/grok.h -- -std=c++11 -x c++ \
    -I./external/grok/src/lib/jp2 \
    -I./external/grok/src/lib/jp2/codestream \
    -I./external/grok/src/lib/jp2/codestream/markers \
    -I./external/grok/src/lib/jp2/coding \
    -I./external/grok/src/lib/jp2/common \
    -I./external/grok/src/lib/jp2/transform \
    -I./external/grok/src/lib/jp2/util \
    -I./external/grok/src/lib/jp2/util \
    -I./external/grok/src/lib/jp2_plugin \
    -I./external/grok/src/lib/jp2/cache \
    -I./external/grok/src/lib/jp2/highway \
    -I./external/grok/src/lib/jp2/plugin \
    -I./external/grok/src/lib/jp2/t1 \
    -I./external/grok/src/lib/jp2/t1/OJPH \
    -I./external/grok/src/lib/jp2/t1/OJPH/coding \
    -I./external/grok/src/lib/jp2/t1/OJPH/common \
    -I./external/grok/src/lib/jp2/t1/OJPH/others \
    -I./external/grok/src/lib/jp2/t1/part1 \
    -I./external/grok/src/lib/jp2/t1/part1/impl \
    -I./external/grok/src/lib/jp2/t1/part15 \
    -I./external/grok/src/lib/jp2/t1/part15/codestream \
    -I./external/grok/src/lib/jp2/t1/part15/coding \
    -I./external/grok/src/lib/jp2/t1/part15/common \
    -I./external/grok/src/lib/jp2/t1/part15/transform \
    -I./external/grok/src/lib/jp2/t2 \
    -I./external/grok/src/lib/jp2/tile \


    "$@"
