#!/usr/bin/env bash
set -e
set -o pipefail
set -x

source .travis-common.sh

make "${MAKEFLAGS[@]}" NANOPB_DIR=nanopb all
make "${MAKEFLAGS[@]}" NANOPB_DIR=nanopb GTEST_DIR="$LOCAL_PREFIX" check
make "${MAKEFLAGs[@]}" NANOPB_DIR=nanopb PREFIX="$LOCAL_PREFIX" install

test -f "$LOCAL_PREFIX"/lib/libpblog.so
test -f "$LOCAL_PREFIX"/lib/libpblog.a
test -d "$LOCAL_PREFIX"/include/pblog
