#!/usr/bin/env bash
# Idempotent gcc 11 fixes for Livox-SDK used by livox_sdk_vendor ExternalProject.
# Args: <SOURCE_DIR>
set -euo pipefail
SRC="${1:?SOURCE_DIR required}"
CMAKE="$SRC/sdk_core/CMakeLists.txt"
HDR="$SRC/sdk_core/src/base/thread_base.h"

if [ -f "$CMAKE" ] && grep -q -- '-Werror' "$CMAKE"; then
  sed -i \
    's/-Wall -Werror -Wno-c++11-long-long/-Wall/; s/-Wno-unknown-pragmas -Wall -Werror -Wno-c++11-long-long/-Wno-unknown-pragmas -Wall/' \
    "$CMAKE"
fi
if [ -f "$HDR" ] && ! grep -q '#include <memory>' "$HDR"; then
  sed -i 's|#include "noncopyable.h"|#include <memory>\n#include "noncopyable.h"|' "$HDR"
fi
exit 0
