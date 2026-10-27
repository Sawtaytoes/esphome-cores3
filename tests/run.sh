#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
check_dir=$(mktemp -d)
trap 'rm -rf "$check_dir"' EXIT HUP INT TERM
cxx=${CXX:-g++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined tests/protocol_test.cpp -o "$check_dir/protocol"
"$check_dir/protocol"
"$cxx" -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable -fsanitize=address,undefined -Itests/stubs tests/lifecycle_test.cpp components/camera_link_sender/camera_link_sender.cpp components/camera_link_viewer/camera_link_viewer.cpp -o "$check_dir/lifecycle"
"$check_dir/lifecycle"
if [ -n "${JPEGDEC_SOURCE:-}" ]; then
  # Actual upstream decoder has documented UBSan findings; keep ASan enabled.
  "$cxx" -std=c++17 -Wall -Wextra -Wno-unused-variable -fsanitize=address \
    -D__LINUX__ -DCAMERA_LINK_REAL_JPEGDEC -Itests/stubs -I"$JPEGDEC_SOURCE/src" \
    tests/viewer_decoder_test.cpp components/camera_link_viewer/camera_link_viewer.cpp \
    "$JPEGDEC_SOURCE/src/JPEGDEC.cpp" -o "$check_dir/decoder"
  "$check_dir/decoder"
else
  echo 'Real JPEGDEC checks skipped: set JPEGDEC_SOURCE to run them.'
fi
