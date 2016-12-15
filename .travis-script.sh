#!/usr/bin/env bash
set -e
set -o pipefail
set -x

source .travis-common.sh

clang-tidy() {
  clang-tidy${CC:5} "$@" >tmp-tidy-results || true
  set +x
  if [ -n "$(cat tmp-tidy-results)" ]; then
    echo "########## $1 ##########" >> tidy-results
    cat tmp-tidy-results >> tidy-results
  fi
  set -x
}

if [ "$LINT" = "1" ]; then
  make "${MAKEFLAGS[@]}" NANOPB_DIR=../nanopb all
  clang-tidy -dump-config
  clang-tidy -explain-config
  touch tidy-results
  for file in $(find include src -name \*.c -or -name \*.h); do
    clang-tidy "$file" -- -std=gnu11 -I.pblog/include
  done
  for file in $(find test -name \*.cc -or -name \*.hh); do
    clang-tidy "$file" -- -std=gnu++11 -I.pblog/include -I"$LOCAL_PREFIX"/include
  done
  set +x
  if [ -n "$(cat tidy-results)" ]; then
    cat tidy-results
    exit 1
  fi
  set -x
else
  make "${MAKEFLAGS[@]}" NANOPB_DIR=../nanopb all
  make "${MAKEFLAGS[@]}" NANOPB_DIR=../nanopb GTEST_DIR="$LOCAL_PREFIX" check
  make "${MAKEFLAGs[@]}" NANOPB_DIR=../nanopb PREFIX="$LOCAL_PREFIX" install

  test -f "$LOCAL_PREFIX"/lib/libpblog.so
  test -f "$LOCAL_PREFIX"/lib/libpblog.a
  test -d "$LOCAL_PREFIX"/include/pblog
fi
