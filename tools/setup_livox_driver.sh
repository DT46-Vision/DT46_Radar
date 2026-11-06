#!/usr/bin/env bash
# Ensure src/livox_ros2_driver exists and wire Ubuntu 22.04 / gcc 11 SDK patch.
# Driver tree is vendored in-repo; this script refreshes the vendor ExternalProject patch.
# Wiring / SN / IP: Livox_MID-70 README only.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/src/livox_ros2_driver"
PATCH_SRC="$ROOT/tools/patches/livox-sdk-ubuntu2204-gcc11.patch"
APPLY_SH="$ROOT/tools/apply_livox_sdk_gcc11_patch.sh"
REPO="https://github.com/Livox-SDK/livox_ros2_driver.git"

if [ ! -d "$SRC" ]; then
  echo "== clone $REPO"
  git clone --depth 1 "$REPO" "$SRC"
elif [ -d "$SRC/.git" ]; then
  echo "== reuse $SRC @ $(git -C "$SRC" rev-parse --short HEAD)"
else
  echo "== reuse vendored $SRC (tracked in this repo)"
fi


VENDOR_CMAKE="$SRC/livox_sdk_vendor/CMakeLists.txt"
VENDOR_PATCH_DIR="$SRC/livox_sdk_vendor/patches"
mkdir -p "$VENDOR_PATCH_DIR"
cp -f "$PATCH_SRC" "$VENDOR_PATCH_DIR/livox-sdk-ubuntu2204-gcc11.patch"
cp -f "$APPLY_SH" "$VENDOR_PATCH_DIR/apply_livox_sdk_gcc11_patch.sh"
chmod +x "$VENDOR_PATCH_DIR/apply_livox_sdk_gcc11_patch.sh"

if ! grep -q 'apply_livox_sdk_gcc11_patch.sh' "$VENDOR_CMAKE"; then
  echo "== patch livox_sdk_vendor ExternalProject"
  python3 - "$VENDOR_CMAKE" <<'PY'
import sys
from pathlib import Path
path = Path(sys.argv[1])
text = path.read_text()
needle = "TIMEOUT 60\n    CMAKE_ARGS"
insert = """TIMEOUT 60
    PATCH_COMMAND
      ${CMAKE_CURRENT_SOURCE_DIR}/patches/apply_livox_sdk_gcc11_patch.sh
      <SOURCE_DIR>
    CMAKE_ARGS
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5"""
# CMAKE_ARGS line is shared; only inject PATCH once, then ensure policy arg.
if "apply_livox_sdk_gcc11_patch.sh" not in text:
    if needle not in text:
        raise SystemExit("vendor CMakeLists.txt TIMEOUT/CMAKE_ARGS pattern not found")
    text = text.replace(needle, insert, 1)
    print("patched PATCH_COMMAND", path)
if "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" not in text:
    text = text.replace(
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON\n",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON\n"
        "      -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n",
        1,
    )
    print("patched CMAKE_POLICY", path)
path.write_text(text)
PY
else
  echo "== vendor CMake already has patch hook"
  if ! grep -q 'CMAKE_POLICY_VERSION_MINIMUM' "$VENDOR_CMAKE"; then
    python3 - "$VENDOR_CMAKE" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
if "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" not in text:
    text = text.replace(
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON\n",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON\n"
        "      -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n",
        1,
    )
    path.write_text(text)
    print("patched CMAKE_POLICY", path)
PY
  fi
fi

echo "== done. Next: source /opt/ros/humble/setup.bash && colcon build --packages-up-to livox_ros2_driver"
