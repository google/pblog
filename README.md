pblog - Protocol Buffer Log Library
===================================

![alt text](https://travis-ci.org/google/pblog.svg?branch=master "TravisCI Status")

pblog is a small, low overhead, structured logging library intended to be used
to log firmware events. It is based on protobufs and uses the nanopb
implementation in order to tackle object size concerns.

Dependencies
------------
Runtime

- c compiler
- make
- protobuf         https://github.com/google/protobuf
- python-protobuf  https://pypi.python.org/pypi/protobuf

Testing
- c++ compiler

For ubuntu systems these can all be installed with apt

    apt install make protobuf-compiler python-protobuf

Building
--------
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> all

if you don't already have nanopb you can

    git clone https://github.com/nanopb/nanopb
	make -C nanopb/generator/proto
	make NANOPB_DIR=nanopb all

Installing
----------
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> PREFIX=/usr install

Testing
-------
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> GTEST_DIR=<GTEST_DIR> check

if you don't already have gtest you can run

    git clone https://github.com/google/googletest
    pushd "googletest" >/dev/null
    cmake \
      -DCMAKE_INSTALL_PREFIX="$(pwd)/googletest" \
      -DMAKE_BUILD_TYPE=Release \
      -DBUILD_GTEST=ON -DBUILD_GMOCK=OFF
    make
    make install
    popd >/dev/null
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> GTEST_DIR=googletest check

Use in a project
----------------
If you would like to build pblog into your project, we provide a makefile
mk/pblog.mk which can be included.

The makefile depends on the following variables:

- NANOPB\_DIR: The directory containing the source code for nanopb
- PBLOG\_BUILD\_STATIC: Whether or not we should build a static pblog
- PBLOG\_BUILD\_SHARED: Whether or not we should build a shared pblog

The makefile is guaranteed to export the following variables:

- PBLOG\_LIBRARIES: The targets from the enabled pblogging libraries
- PBLOG\_STATIC: The target for the static pblog library
- PBLOG\_SHARED: The target for the shared pblog library
