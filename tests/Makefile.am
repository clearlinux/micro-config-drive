if BUILD_TESTS

SUBDIRS = .

ACLOCAL_AMFLAGS = -I m4

TESTS =

check_PROGRAMS =

check_LTLIBRARIES = libtest.la
COMMON_CFLAGS = -std=gnu99 -I$(top_srcdir)/src $(CHECK_FLAGS) $(GLIB_CFLAGS) $(CURL_CFLAGS)
COMMON_LDADD = $(CHECK_LIBS) $(GLIB_LIBS) $(CURL_LIBS)

libtest_la_SOURCES = \
	../src/lib.c \
	../src/curl.c

libtest_la_CFLAGS = $(COMMON_CFLAGS)

lib_test_SOURCES = lib_test.c
lib_test_CFLAGS = $(COMMON_CFLAGS)
lib_test_LDADD = $(COMMON_LDADD) libtest.la
TESTS += lib_test

curl_test_SOURCES = curl_test.c
curl_test_CFLAGS = $(COMMON_CFLAGS)
curl_test_LDADD = $(COMMON_LDADD) libtest.la
TESTS += curl_test

check_PROGRAMS += $(TESTS)

CLEANFILES = *~ *.log

endif
