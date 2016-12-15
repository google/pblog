# Directory and Install parameters
DESTDIR ?=
PREFIX ?= /usr
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib

# Test dependencies
GTEST_DIR ?= /usr
GTEST_LIBDIR ?= $(GTEST_DIR)/lib
GTEST_INCDIR ?= $(GTEST_DIR)/include

# Command overrides
INSTALL ?= install
CP ?= cp
CXX ?= c++

# Compiler Options
CFLAGS ?= -Wall -Werror

# Build Options
BUILD_STATIC ?= y
BUILD_SHARED ?= y

PBLOG_BUILD_STATIC = $(BUILD_STATIC)
PBLOG_BUILD_SHARED = $(BUILD_SHARED)

# Test enumeration
PBLOG_TESTS_SRC = $(wildcard $(PBLOG_DIR)/test/*_test.cc)
PBLOG_TESTS_HEADERS = $(PBLOG_HEADERS) $(wildcard $(PBLOG_DIR)/test/*.h)
PBLOG_TESTS_COMMON_FILES = $(filter-out %_test.cc,$(wildcard $(PBLOG_DIR)/test/*.cc))
PBLOG_TESTS_COMMON_OBJECTS = $(patsubst $(PBLOG_DIR)/test/%.cc,$(PBLOG_OUT)/test/%.o,$(PBLOG_TESTS_COMMON_FILES))
PBLOG_TESTS = $(patsubst $(PBLOG_DIR)/test/%.cc,$(PBLOG_OUT)/%,$(PBLOG_TESTS_SRC))
PBLOG_TESTS_RUN = $(patsubst %,%_run,$(PBLOG_TESTS))

# Test Params
PBLOG_TESTS_CFLAGS = $(CFLAGS) $(PBLOG_CFLAGS) -std=gnu++11 \
					 -I$(PBLOG_INCLUDE) -I$(GTEST_INCDIR) \
					 -Wl,-rpath $(PBLOG_OUT) -Wl,-rpath $(GTEST_LIBDIR)
PBLOG_TESTS_LIBS = -L$(PBLOG_OUT) -lpblog -L$(GTEST_LIBDIR) -lgtest_main \
				   -lgtest -pthread

.SECONDARY: $(PBLOG_TESTS) $(PBLOG_SECONDARY)
.PHONY: all all-real check install clean $(PBLOG_PHONY)

# We need this special rule to make sure all comes before rules in pblog.mk
all: all-real

include mk/pblog.mk

# Rule for building common test objects
$(PBLOG_OUT)/test/%.o: $(PBLOG_DIR)/test/%.cc $(PBLOG_TESTS_HEADERS)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)/test
	$(CXX) $(PBLOG_TESTS_CFLAGS) -c $< -o $@

# Rule for building test cases
$(PBLOG_OUT)/%_test: $(PBLOG_DIR)/test/%_test.cc $(PBLOG_TESTS_COMMON_OBJECTS) $(PBLOG_TESTS_HEADERS) $(PBLOG_LIBRARIES)
	@$(PBLOG_MKDIR) -p $(PBLOG_OUT)
	$(CXX) $(PBLOG_TESTS_CFLAGS) $< -o $@ $(PBLOG_TESTS_COMMON_OBJECTS) $(PBLOG_TESTS_LIBS)

# Rule for running test cases
$(PBLOG_OUT)/%_run: $(PBLOG_OUT)/%
	$<
	touch $@

all-real: $(PBLOG_LIBRARIES) $(PBLOG_HEADERS)

check: $(PBLOG_TESTS_RUN)

install: $(PBLOG_LIBRARIES) $(PBLOG_HEADERS)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(PBLOG_LIBRARIES) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(INCLUDEDIR)
	$(CP) -r $(PBLOG_INCLUDE)/* $(DESTDIR)$(INCLUDEDIR)/

clean: pblog_clean
