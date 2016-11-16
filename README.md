pblog - Protocol Buffer Log Library
===================================

pblog is a small, low overhead, structured logging library intended to be used
to log firmware events. It is based on protobufs and uses the nanopb
implementation in order to tackle object size concerns.

Dependencies
------------
Runtime

- protobuf
- make
- nanopb

Source

- nanopb

Building
--------
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> all

Installing
----------
    make NANOPB_DIR=<NANOPB_SOURCE_DIR> PREFIX=/usr install

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
