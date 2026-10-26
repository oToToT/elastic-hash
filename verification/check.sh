#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
lean -DwarningAsError=true verification/Scheduler.lean
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Wpedantic -I. \
  elastic_hash_regression_test.cc -o "$work/regressions"
"$work/regressions"
