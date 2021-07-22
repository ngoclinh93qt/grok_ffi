#!/bin/sh
exec bindgen \
    --no-prepend-enum-name \
    --allowlist-function 'grk_.*' \
    --blocklist-type 'FILE' \
    --blocklist-type '_IO_FILE' \
    --raw-line 'use libc::FILE as FILE;' \
    --size_t-is-usize \
    -o src/lib.rs \
    ./external/grok/src/lib/jp2/grok.h -- -std=c++11 -x c++  \
    "$@"
