if BUILD_TESTS

SUBDIRS = .

ACLOCAL_AMFLAGS = -I m4

TESTS =

check_PROGRAMS =

check_LTLIBRARIES = libtest.la

COMMON_CFLAGS = -I$(top_srcdir)/src $(GLIB_CFLAGS)
TEST_CFLAGS = $(COMMON_CFLAGS) $(CHECK_FLAGS)
TEST_LDADD = $(CHECK_LIBS) $(GLIB_LIBS)

libtest_la_SOURCES = ../src/lib.c
libtest_la_CFLAGS = $(COMMON_CFLAGS)

lib_test_SOURCES = lib_test.c
lib_test_CFLAGS = $(TEST_CFLAGS)
lib_test_LDADD = $(TEST_LDADD) libtest.la
TESTS += lib_test

check_PROGRAMS += $(TESTS)

CLEANFILES = *~ *.log

endif
