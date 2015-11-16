#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([clr-cloud-init],[1],[dev@clearlinux.org])
AM_INIT_AUTOMAKE([foreign -Wall -Werror -Wno-portability silent-rules subdir-objects color-tests no-dist-gzip dist-xz])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_FILES([Makefile
		data/cloud-init.service])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MKDIR_P

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.24.1])
PKG_CHECK_MODULES([YAML], [yaml-0.1 >= 0.1.4])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0])
PKG_CHECK_MODULES([PARTED], [libparted = 3.2])

# Checks for header files.
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([glib.h])
AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([stdbool.h])
AC_CHECK_HEADERS([stdio.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [enable debug mode @<:@default=no@:>@]),
	      [], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"],
	[AC_DEFINE([DEBUG], [1], [Debugging mode enabled])],
	[AC_DEFINE([NDEBUG], [1], [Debugging and assertions disabled])])
AM_CONDITIONAL([DEBUG], [test x$enable_debug = x"yes"])

# Checks for commands
AC_CHECK_PROG([RESIZE2FS], [resize2fs], [yes])
if test x"$RESIZE2FS" != x"yes" ; then
   AC_MSG_ERROR([Please install resize2fs.])
fi

# Options
AC_ARG_WITH([systemdsystemunitdir], AS_HELP_STRING([--with-systemdsystemunitdir=DIR],
	[path to systemd system service directory]), [path_systemdsystemunit=${withval}],
	[path_systemdsystemunit="`$PKG_CONFIG --variable=systemdsystemunitdir systemd`"])
	SYSTEMD_SYSTEMUNITDIR="${path_systemdsystemunit}"
AC_SUBST(SYSTEMD_SYSTEMUNITDIR)
AM_CONDITIONAL(SYSTEMD, test -n "${path_systemdsystemunit}")
		

# Checks for library functions.

AC_OUTPUT
