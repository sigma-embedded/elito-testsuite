abs_top_srcdir = $(abspath $(top_srcdir))
abs_top_builddir = $(abspath $(top_builddir))

prefix = /usr/local
bindir = $(prefix)/bin
datadir = $(prefix)/share
libdir = $(prefix)/lib
libexecdir = $(prefix)/libexec

pkgdatadir = $(datadir)/$(PACKAGE_NAME)
pkglibexecdir = $(libexecdir)/$(PACKAGE_NAME)
pkglibdir = $(libdir)/$(PACKAGE_NAME)

SED = sed

INSTALL = install
INSTALL_SCRIPTS = $(INSTALL) -p -m 0755
INSTALL_PROGRAMS = $(INSTALL) -p -m 0755
INSTALL_DATA = $(INSTALL) -p -m 0644
MKDIR_P = $(INSTALL) -d -m 0755

## $(call register_directory,<dir-name>)
define register_directory
ifeq ($$(_registered_directory_$1),)
_registered_directory_$1 := 1
$$(DESTDIR)$$($(1)dir):
	$$(MKDIR_P) $$@
endif
endef

define register_install_location_single
$$(DESTDIR)$1/$$(notdir $2):	$2
all:		$2
endef

## $(call register_install_location,<dir-name>,<filetype>)
define register_install_location
_$1_$2 = $$(addprefix $$($(1)dir)/,$$(notdir $$($1_$2)))
$$(foreach p,$$($1_$2),\
  $$(eval $$(call register_install_location_single,$$($(1)dir),$$p)))

$$(addprefix $$(DESTDIR),$$(_$1_$2)): | $$(DESTDIR)$$($(1)dir)
	$$(INSTALL_$2) $$^ $$@

$(eval $(call register_directory,$1))
_install-$1_$2: $$(_$1_$2)
install:	_install-$1_$2
endef


define build_c_program
$1:	private _c_opts = $(foreach O,CPP C LD, $$(AM_$(O)FLAGS) $$($(O)FLAGS) $$($1_$(O)FLAGS))
$1:	$$($1_SOURCES) $$($1_LDADD)
	$$(CC) $$(_c_opts) $$(filter %.c,$$^) -o $$@ $$(AM_LIBS) $$(LIBS) $$($1_LDADD)
endef
