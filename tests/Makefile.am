if BUILD_TESTS

SUBDIRS = .

ACLOCAL_AMFLAGS = -I m4

TESTS =

check_PROGRAMS =

check_LTLIBRARIES = libtest.la
COMMON_CFLAGS = -std=gnu99 -I$(top_srcdir)/src -I$(top_srcdir)/src/ccmodules \
	-I$(top_srcdir)/src/interpreters \
	$(CHECK_FLAGS) $(GLIB_CFLAGS) $(CURL_CFLAGS) $(YAML_CFLAGS) $(BLKID_CFLAGS) $(PARTED_CFLAGS)
COMMON_LDADD = $(CHECK_LIBS) $(GLIB_LIBS) $(CURL_LIBS) $(YAML_LIBS) $(BLKID_LIBS) $(PARTED_LIBS)

libtest_la_SOURCES = \
	../src/lib.c \
	../src/async_task.c \
	../src/disk.c \
	../src/userdata.c \
	../src/interpreters/cloud_config.c \
	../src/interpreters/shell_script.c \
	../src/ccmodules/envar.c \
	../src/ccmodules/groups.c \
	../src/ccmodules/hostname.c \
	../src/ccmodules/package_upgrade.c \
	../src/ccmodules/packages.c \
	../src/ccmodules/runcmd.c \
	../src/ccmodules/service.c \
	../src/ccmodules/ssh_authorized_keys.c \
	../src/ccmodules/users.c \
	../src/ccmodules/write_files.c \
	../src/ccmodules/fbootcmd.c \
	../src/ccmodules/wait-for-network.c

if DEBUG
libtest_la_SOURCES += ../src/debug.c
endif


libtest_la_CFLAGS = $(COMMON_CFLAGS)

lib_test_SOURCES = lib_test.c
lib_test_CFLAGS = $(COMMON_CFLAGS) $(AM_CFLAGS)
lib_test_LDADD = libtest.la $(COMMON_LDADD)
TESTS += lib_test

userdata_test_SOURCES = userdata_test.c
userdata_test_CFLAGS = $(COMMON_CFLAGS) $(AM_CFLAGS)
userdata_test_LDADD = libtest.la $(COMMON_LDADD)
TESTS += userdata_test


check_PROGRAMS += $(TESTS)

CLEANFILES = *~ *.log

endif
