SUBDIRS = . tests

ACLOCAL_AMFLAGS=-I m4

AM_CPPFLAGS = -I $(top_srcdir)/src -I $(top_srcdir)/src/interpreters -I $(top_srcdir)/src/datasources
AM_CFLAGS = -std=gnu99 -fstack-protector -Wall -pedantic \
	-Wstrict-prototypes -Wundef -fno-common \
	-Wformat -Wformat-security \
	-Wconversion -Wunused-variable -Wunreachable-code

EXTRA_DIST = \
	LICENSE \
	COPYING \
	data/cloud-init.service.in \
	data/cloud-init-pre-network.service.in

DISTCHECK_CONFIGURE_FLAGS =  \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir) --enable-debug

dist_man_MANS = \
	docs/cloud-init.1 \
	docs/cloud-config.5

bin_PROGRAMS = cloud-init
cloud_init_SOURCES = \
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
	src/curl.c \
	src/curl.h \
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

if ENABLE_WERROR
AM_CFLAGS += -Werror
endif
if DEBUG
AM_CFLAGS += -ggdb3 -O0
cloud_init_SOURCES += src/debug.c
endif

cloud_init_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(YAML_CFLAGS) $(JSON_GLIB_CFLAGS) $(PARTED_CFLAGS) $(BLKID_CFLAGS)
cloud_init_LDADD = $(GLIB_LIBS) $(YAML_LIBS) $(CURL_LIBS) $(JSON_GLIB_LIBS) $(PARTED_LIBS) $(BLKID_LIBS)
SYSTEMD_DIR=$(prefix)/lib/systemd/system/
systemdsystemunitdir = @SYSTEMD_SYSTEMUNITDIR@
systemdsystemunit_DATA = data/cloud-init.service data/cloud-init-pre-network.service

systemdsystemunit-install-local:
	mkdir -p $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/
	ln -sf ../cloud-init.service $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/cloud-init.service
	ln -sf ../cloud-init-pre-network.service $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/cloud-init-pre-network.service

install-data-local: systemdsystemunit-install-local

systemdsystemunit-uninstall-local:
	rm -f $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/cloud-init.service
	rm -f $(DESTDIR)$(systemdsystemunitdir)/multi-user.target.wants/cloud-init-pre-network.service

uninstall-local: systemdsystemunit-uninstall-local
