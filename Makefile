top_srcdir ?= .
top_builddir ?= .

PACKAGE_NAME = elito-testsuite
PACKAGE_VERSION = 0.0.1

AM_CFLAGS = -std=gnu99
AM_CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -Wall -W -O2 -g -fstack-protector -Wno-unused-parameter -Wstrict-prototypes -Werror
CPPFLAGS = -D_FORTIFY_SOURCE=2

include $(top_srcdir)/mk/common.mk

testdir = $(pkgdatadir)/tests

VPATH += $(top_srcdir)

bin_SCRIPTS = subst/runtests
pkglibexec_PROGRAMS = \
	runtest \
	check-file \
	read-write \

pkglibexec_SCRIPTS = \
	nand-crc-test

pkgdata_DATA = subst/runtests.mk functions

test_DATA = \
	tests/_core.catorder \
	tests/_selftest-0000.test \
	tests/_selftest-0001.test \
	tests/_selftest-0002.test \
	tests/_core-0000.test \

runtest_SOURCES = \
	src/pipe.h \
	src/runtest.c \
	src/subprocess.c \
	src/subprocess.h \
	src/util.h

check-file_SOURCES = \
	src/check-file.c

read-write_SOURCES = \
	src/read-write.c

_sed_cmd = \
  -e 's!@PKGLIBEXECDIR@!$(pkglibexecdir)!g' \
  -e 's!@PKGDATADIR@!$(pkgdatadir)!g' \
  -e 's!@PKGLIBDIR@!$(pkglibdir)!g' \
  -e '/^\#\#\# development line \#\#\#/,+1d'

.DEFAULT_GOAL = all

$(eval $(call register_install_location,bin,SCRIPTS))
$(eval $(call register_install_location,bin,PROGRAMS))
$(eval $(call register_install_location,pkglibexec,PROGRAMS))
$(eval $(call register_install_location,pkglibexec,SCRIPTS))
$(eval $(call register_install_location,pkgdata,DATA))
$(eval $(call register_install_location,test,DATA))

$(eval $(call build_c_program,runtest))
$(eval $(call build_c_program,check-file))
$(eval $(call build_c_program,read-write))

subst:
	$(MKDIR_P) $@

subst/%:	% | subst
	rm -f $@
	$(SED) $(_sed_cmd) < $< > $@.tmp
	mv $@.tmp $@
	rm -f $@.tmp

runtest:	$(runtest_SOURCES)

all install:	
