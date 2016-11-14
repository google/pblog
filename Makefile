# Directory and Install parameters
DESTDIR ?=
PREFIX ?= /usr
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib

# Command overrides
INSTALL ?= install
CP ?= cp

# Build Options
BUILD_STATIC ?= y
BUILD_SHARED ?= y

PBLOG_BUILD_STATIC = $(BUILD_STATIC)
PBLOG_BUILD_SHARED = $(BUILD_SHARED)

.PHONY: all test install clean all-real

# We need this special rule to make sure all comes before rules in pblog.mk
all: all-real

include mk/pblog.mk

all-real: $(PBLOG_LIBRARIES) $(PBLOG_HEADERS)

test: $(PBLOG_LIBRARIES) $(PBLOG_HEADERS)

install: $(PBLOG_LIBRARIES) $(PBLOG_HEADERS)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(PBLOG_LIBRARIES) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(INCLUDEDIR)
	$(CP) -r $(PBLOG_INCLUDE)/* $(DESTDIR)$(INCLUDEDIR)/

clean: pblog_clean
