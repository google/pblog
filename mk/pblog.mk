# An include file for Makefiles. It provides rules for building
# a static libpblog.a based on protobuf and nanopb.

# Path to the pblog root directory
PBLOG_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../)
PBLOG_OUT := $(CURDIR)/.pblog

# Path to nanopb. Make sure it's accessible
ifndef NANOPB_DIR
$(error You must provide a NANOPB_DIR that contains the nanopb package sources.)
endif
include $(NANOPB_DIR)/extra/nanopb.mk

# Build Options
PBLOG_BUILD_STATIC ?= y
PBLOG_BUILD_SHARED ?= n

PBLOG_BUILD_MODULE_FILE ?= y

# Parameters
PBLOG_LIBRARIES =
PBLOG_STATIC = $(PBLOG_OUT)/libpblog.a
ifeq ($(BUILD_STATIC),y)
PBLOG_LIBRARIES += $(PBLOG_STATIC)
endif
PBLOG_SHARED = $(PBLOG_OUT)/libpblog.so
ifeq ($(BUILD_SHARED),y)
PBLOG_LIBRARIES += $(PBLOG_SHARED)
endif

# Command substitution
PBLOG_CC = $(if $(CC),$(CC),cc)
PBLOG_MKDIR = $(if $(MKDIR),$(MKDIR),mkdir)
PBLOG_CP = $(if $(CP),$(CP),cp)
PBLOG_AR = $(if $(AR),$(AR),ar)

# We need certain CPP / C flags for correctness of struct generation
CPPFLAGS += -DPB_FIELD_32BIT=1
CFLAGS += -DPB_FIELD_32BIT=1

PBLOG_CFLAGS = $(CPPFLAGS) $(CFLAGS) -Wall -DPB_FIELD_32BIT=1
ifeq ($(PBLOG_BUILD_SHARED),y)
PBLOG_CFLAGS += -fPIC
endif

HEADER_FILTER =
SOURCE_FILTER =
ifeq ($(PBLOG_BUILD_MODULE_FILE),n)
HEADER_FILTER += %/file.h
SOURCE_FILTER += %/file.c
endif

PBLOG_SRC_INCLUDE = $(PBLOG_DIR)/include
PBLOG_SRC_HEADERS = $(filter-out $(HEADER_FILTER),$(wildcard $(PBLOG_SRC_INCLUDE)/pblog/*.h))
PBLOG_SRC_PROTOS = $(wildcard $(PBLOG_DIR)/proto/*.proto)
PBLOG_SRC_FILES = $(filter-out $(SOURCE_FILTER),$(wildcard $(PBLOG_DIR)/src/*.c))

PBLOG_INCLUDE = $(PBLOG_OUT)/include
PBLOG_ONLY_HEADERS = $(patsubst $(PBLOG_SRC_INCLUDE)/%,$(PBLOG_INCLUDE)/%,$(PBLOG_SRC_HEADERS))
PBLOG_PROTO_HEADERS = $(patsubst $(PBLOG_DIR)/proto/%.proto,$(PBLOG_INCLUDE)/pblog/%.pb.h,$(PBLOG_SRC_PROTOS))
PBLOG_NANOPB_HEADERS = $(patsubst $(NANOPB_DIR)/%.h,$(PBLOG_INCLUDE)/nanopb/%.h,$(wildcard $(NANOPB_DIR)/*.h))
PBLOG_HEADERS = $(PBLOG_NANOPB_HEADERS) $(PBLOG_ONLY_HEADERS) $(PBLOG_PROTO_HEADERS)
PBLOG_ONLY_OBJECTS = $(patsubst $(PBLOG_DIR)/src/%.c,$(PBLOG_OUT)/pblog/%.o,$(PBLOG_SRC_FILES))
PBLOG_PROTO_OBJECTS = $(patsubst $(PBLOG_DIR)/proto/%.proto,$(PBLOG_OUT)/pblog/%.pb.o,$(PBLOG_SRC_PROTOS))
PBLOG_NANOPB_OBJECTS = $(patsubst $(NANOPB_DIR)/%.c,$(PBLOG_OUT)/nanopb/%.o,$(NANOPB_CORE))
PBLOG_OBJECTS = $(PBLOG_NANOPB_OBJECTS) $(PBLOG_ONLY_OBJECTS) $(PBLOG_PROTO_OBJECTS)

.SECONDARY: $(PBLOG_HEADERS)

# Nanopb headers
$(PBLOG_INCLUDE)/nanopb/%.h: $(NANOPB_DIR)/%.h
	@$(PBLOG_MKDIR) -p $(PBLOG_INCLUDE)/nanopb
	$(PBLOG_CP) $< $@

# Pblog headers
$(PBLOG_INCLUDE)/pblog/%.h: $(PBLOG_SRC_INCLUDE)/pblog/%.h
	@$(PBLOG_MKDIR) -p $(PBLOG_INCLUDE)/pblog
	$(PBLOG_CP) $< $@

# Protobuf code generation
$(PBLOG_OUT)/pblog/%.pb.c $(PBLOG_OUT)/pblog/%.pb.h: $(PBLOG_DIR)/proto/%.proto
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)/pblog
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out="-L '#include <nanopb/%s>':$(PBLOG_OUT)/pblog" -I$(PBLOG_DIR)/proto -I$(NANOPB_DIR)/generator/proto $<

# Pblog proto headers
$(PBLOG_INCLUDE)/pblog/%.pb.h: $(PBLOG_OUT)/pblog/%.pb.h
	$(PBLOG_CP) $< $@

# Pblog proto sources
$(PBLOG_OUT)/pblog/%.pb.o: $(PBLOG_OUT)/pblog/%.pb.c $(PBLOG_PROTO_HEADERS) $(PBLOG_NANOPB_HEADERS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)/pblog
	$(PBLOG_CC) $(PBLOG_CFLAGS) -I$(PBLOG_INCLUDE) -I$(PBLOG_INCLUDE)/pblog -c $< -o $@

# Nanopb sources
$(PBLOG_OUT)/nanopb/%.o: $(NANOPB_DIR)/%.c $(PBLOG_NANOPB_HEADERS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)/nanopb
	$(PBLOG_CC) $(PBLOG_CFLAGS) -c $< -o $@

# Pblog sources
$(PBLOG_OUT)/pblog/%.o: $(PBLOG_DIR)/src/%.c $(PBLOG_HEADERS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)/pblog
	$(PBLOG_CC) $(PBLOG_CFLAGS) -I$(PBLOG_INCLUDE) -c $< -o $@

# Libraries
$(PBLOG_OUT)/libpblog.a: $(PBLOG_OBJECTS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)
	$(PBLOG_AR) rcs $(PBLOG_OUT)/libpblog.a $(PBLOG_OBJECTS)

$(PBLOG_OUT)/libpblog.so: $(PBLOG_OBJECTS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)
	$(PBLOG_CC) -shared -Wl,-soname,libpblog.so $(PBLOG_OBJECTS) -o $(PBLOG_OUT)/libpblog.so

.PHONY: pblog_clean
pblog_clean:
	rm -rf $(PBLOG_OUT)
