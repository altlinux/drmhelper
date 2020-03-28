# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
#

PROJECT = drmhelper
VERSION = $(shell sed '/^Version: */!d;s///;q' libdrmhelper.spec)
MAJOR = 0

SHAREDLIB = lib$(PROJECT).so
SONAME    = $(SHAREDLIB).$(MAJOR)
STATICLIB = lib$(PROJECT).a
MAP       = lib$(PROJECT).map

TARGETS = $(PROJECT) $(SHAREDLIB) $(STATICLIB)

INSTALL = install

prefix       = /usr
exec_prefix  = $(prefix)
libdir       = $(exec_prefix)/lib
libexecdir   = $(exec_prefix)/lib
pkgconfigdir = $(libdir)/pkgconfig
includedir   = $(prefix)/include
mandir       = $(prefix)/share/man
man3dir      = $(mandir)/man3
DESTDIR      =

helperdir = $(libexecdir)/$(PROJECT)

WARNINGS = -W -Wall -Waggregate-return -Wcast-align -Wconversion \
	-Wdisabled-optimization -Wmissing-declarations \
	-Wmissing-format-attribute -Wmissing-noreturn \
	-Wmissing-prototypes -Wpointer-arith -Wredundant-decls \
	-Wshadow -Wstrict-prototypes -Wwrite-strings
CPPFLAGS = -std=gnu99 $(WARNINGS) \
	-DLIBEXECDIR=\"$(helperdir)\" \
	-D_GNU_SOURCE
CFLAGS = $(RPM_OPT_FLAGS)
LDLIBS =

TARGETS = drmhelper $(SHAREDLIB) $(STATICLIB) libdrmhelper.pc

all: $(TARGETS)

drmhelper_SRCS    = drmhelper.c ipc.c
libdrmhelper_SRCS = libdrmhelper.c ipc.c

ifneq "$(WITHOUT_SECCOMP)" "1"
CPPFLAGS       += -DUSE_SECCOMP
drmhelper_SRCS += seccomp.c
endif

drmhelper_OBJS    = $(drmhelper_SRCS:.c=.o)
libdrmhelper_OBJS = $(libdrmhelper_SRCS:.c=.o)

DEPS = \
	$(drmhelper_SRCS:.c=.d) \
	$(libdrmhelper_SRCS:.c=.d)

OBJS = \
	$(drmhelper_OBJS) \
	$(libdrmhelper_OBJS)

install: $(TARGETS)
	$(INSTALL) -D -p -m2711 drmhelper $(DESTDIR)$(helperdir)/drmhelper
	$(INSTALL) -D -p -m644 libdrmhelper.h $(DESTDIR)$(includedir)/libdrmhelper.h
	$(INSTALL) -D -p -m644 libdrmhelper.pc $(DESTDIR)$(pkgconfigdir)/libdrmhelper.pc
	$(INSTALL) -D -p -m755 $(SHAREDLIB) $(DESTDIR)$(libdir)/$(SHAREDLIB).$(VERSION)
	$(INSTALL) -D -p -m644 $(STATICLIB) $(DESTDIR)$(libdir)/$(STATICLIB)
	ln -s $(SHAREDLIB).$(VERSION) $(DESTDIR)$(libdir)/$(SONAME)
	ln -s $(SONAME) $(DESTDIR)$(libdir)/$(SHAREDLIB)

clean:
	@rm -f -- $(TARGETS) $(OBJS) $(DEPS) *.os

drmhelper: $(drmhelper_OBJS)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.os: %.c
	$(COMPILE.c) -fPIC $< $(OUTPUT_OPTION)

%.pc: %.pc.in Makefile
	sed \
	    -e 's,@VERSION\@,$(VERSION),g' \
	    -e 's,@prefix\@,$(prefix),g' \
	    -e 's,@exec_prefix\@,$(exec_prefix),g' \
	    -e 's,@libdir\@,$(libdir),g' \
	    -e 's,@includedir\@,$(includedir),g' \
	    < $< > $@

$(SHAREDLIB): $(libdrmhelper_OBJS:.o=.os)
	$(LINK.o) -shared \
		-Wl,-soname,$(SONAME),--version-script=$(MAP),-z,defs,-stats \
		-lc $^ $(OUTPUT_OPTION)

$(STATICLIB): $(libdrmhelper_OBJS)
	$(AR) $(ARFLAGS) $@ $^
	-ranlib $@

# We need dependencies only if goal isn't "clean".
ifneq ($(MAKECMDGOALS),clean)

%.d: %.c Makefile
	@$(CC) -MM $(CPPFLAGS) $< |sed -e 's,\($*\)\.o[ :]*,\1.o $@: Makefile ,g' >$@

ifneq ($(DEPS),)
-include $(DEPS)
endif
endif # clean
