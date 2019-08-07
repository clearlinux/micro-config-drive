SUBDIRS = . tests

ACLOCAL_AMFLAGS=-I m4

AM_CPPFLAGS = -I $(top_srcdir)/src -I $(top_srcdir)/src/interpreters -I $(top_srcdir)/src/datasources
AM_CFLAGS = \
        -fstack-protector -Wall -pedantic \
        -Wstrict-prototypes -Wundef -fno-common \
        -Werror-implicit-function-declaration \
        -Wformat -Wformat-security -Werror=format-security \
        -Wconversion -Wunused-variable -Wunreachable-code \
        -Wall -W -std=gnu99 -DBINDIR=\"$(bindir)\"

EXTRA_DIST = \
	LICENSE \
	COPYING \
	data/ucd.service.in \
	docs/ucd.1.md \
	docs/ucd-data-fetch.1.md \
	docs/cloud-config.5.md

DISTCHECK_CONFIGURE_FLAGS =  \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) --enable-debug

dist_man_MANS = \
	docs/ucd.1 \
	docs/ucd-data-fetch.1 \
	docs/cloud-config.5

bin_PROGRAMS = \
	ucd \
	ucd-data-fetch

ucd_SOURCES = \
	src/ccmodules.h \
	src/ccmodules/groups.c \
	src/ccmodules/package_upgrade.c \
	src/ccmodules/packages.c \
	src/ccmodules/service.c \
	src/ccmodules/ssh_authorized_keys.c \
	src/ccmodules/users.c \
	src/ccmodules/write_files.c \
	src/ccmodules/hostname.c \
	src/ccmodules/runcmd.c \
	src/ccmodules/envar.c \
	src/ccmodules/fbootcmd.c \
	src/datasources.h \
	src/datasources/openstack.c \
	src/datasources/openstack.h \
	src/debug.h \
	src/default_user.h \
	src/handlers.h \
	src/interpreters.h \
	src/interpreters/cloud_config.c \
	src/interpreters/cloud_config.h \
	src/interpreters/shell_script.c \
	src/json.c \
	src/json.h \
	src/lib.c \
	src/lib.h \
	src/main.c \
	src/userdata.c \
	src/userdata.h \
	src/disk.c \
	src/disk.h \
	src/async_task.c \
	src/async_task.h

ucd_data_fetch_SOURCES = \
	src/ucd-data-fetch.c

if ENABLE_WERROR
AM_CFLAGS += -Werror
endif
if DEBUG
AM_CFLAGS += -ggdb3 -O0
ucd_SOURCES += src/debug.c
endif

ucd_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(YAML_CFLAGS) $(JSON_GLIB_CFLAGS) $(PARTED_CFLAGS) $(BLKID_CFLAGS)
ucd_LDADD = $(GLIB_LIBS) $(YAML_LIBS) $(JSON_GLIB_LIBS) $(PARTED_LIBS) $(BLKID_LIBS)

ucd_data_fetch_CFLAGS = $(AM_CFLAGS)

SYSTEMD_DIR=$(prefix)/lib/systemd/system/
systemdsystemunitdir = @SYSTEMD_SYSTEMUNITDIR@
systemdsystemunit_DATA = data/ucd.service \
	data/ucd-aws.service \
	data/ucd-oci.service \
	data/ucd-tencent.service

systemdsystemunit-install-local:
	mkdir -p $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/
	ln -sf ../ucd.service $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/ucd.service

install-data-local: systemdsystemunit-install-local

systemdsystemunit-uninstall-local:
	rm -f $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/ucd.service

uninstall-local: systemdsystemunit-uninstall-local

docs/ucd.1: docs/ucd.1.md
	ronn -r --pipe < docs/ucd.1.md > docs/ucd.1
docs/ucd-data-fetch.1: docs/ucd-data-fetch.1.md
	ronn -r --pipe < docs/ucd-data-fetch.1.md > docs/ucd-data-fetch.1
docs/cloud-config.5: docs/cloud-config.5.md
	ronn -r --pipe < docs/cloud-config.5.md > docs/cloud-config.5
docs: docs/ucd.1 docs/ucd-data-fetch.1 docs/cloud-config.5
