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
