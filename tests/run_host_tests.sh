#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/electronic-load-tests.XXXXXX")
c++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I"$project_dir/main" \
    "$project_dir/tests/host_tests.cpp" \
    "$project_dir/main/sensor_math.cpp" \
    "$project_dir/main/menu_model.cpp" \
    "$project_dir/main/load_control.cpp" \
    "$project_dir/main/system_types.cpp" \
    -o "$test_dir/host_tests"
"$test_dir/host_tests"
