#!/usr/bin/env bash
set -e
set -o pipefail
set -x

source .travis-common.sh

downloadUnpack() {
  local url="$1"
  local dirname="$2"

  mkdir -p "$dirname.tmp"
  curl -L -o "$dirname.file" "$url" || return 1
  tar -xC "$dirname.tmp" -f "$dirname.file" || return 2
  test "$(ls "$dirname.tmp" | wc -l)" -eq "1" || return 3
  mv "$dirname.tmp"/* "$dirname"
  rmdir "$dirname.tmp"
}

# Don't vendor stuff into the source directory
pushd .. >/dev/null

# Setup protoc
PROTOBUF_BASE_URL="https://github.com/google/protobuf/releases/download/v$PROTOBUF_VERSION"
PROTOBUF_URLS=(
  "$PROTOBUF_BASE_URL/protobuf-cpp-$PROTOBUF_VERSION.tar.gz"
  "$PROTOBUF_BASE_URL/protobuf-$PROTOBUF_VERSION.tar.gz"
)
for url in "${PROTOBUF_URLS[@]}"; do
  downloadUnpack "$url" "protobuf" && break
done
pushd "protobuf" >/dev/null
./configure --prefix="$LOCAL_PREFIX"
make "${MAKEFLAGS[@]}"
make "${MAKEFLAGS[@]}" install
popd >/dev/null

# Setup python-protobuf
pip install --user --force-reinstall --ignore-installed --upgrade pip
pip install --user "protobuf==$PROTOBUF_VERSION"

# Setup nanopb
downloadUnpack "https://github.com/nanopb/nanopb/archive/$NANOPB_VERSION.tar.gz" "nanopb"
make "${MAKEFLAGS[@]}" -C nanopb/generator/proto  # TOOD(wak): Remove once we fix this in the makefile

# Setup googletest
downloadUnpack "https://github.com/google/googletest/archive/release-$GOOGLETEST_VERSION.tar.gz" "googletest"
pushd "googletest" >/dev/null
cmake \
  -DCMAKE_INSTALL_PREFIX="$LOCAL_PREFIX" \
  -DMAKE_BUILD_TYPE=Release \
  -DBUILD_GTEST=ON -DBUILD_GMOCK=OFF
make "${MAKEFLAGS[@]}"
make "${MAKEFLAGS[@]}" install
popd >/dev/null

# Pop back into the source directory
popd >/dev/null
