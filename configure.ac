#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([syslog-notify], [0.2], [jtniehof@gmail.com])
AC_CONFIG_SRCDIR([src/syslog-notify.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2 no-dist-gzip])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([errno.h fcntl.h limits.h signal.h stdio.h stdlib.h string.h unistd.h], [],
AC_MSG_ERROR([Required header files missing--are development packages installed?]))

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([memset strchr sleep],[],
AC_MSG_ERROR([Needed function missing from C library. This should not happen.]))

# Check for libnotify/pkgconfig
PKG_CHECK_MODULES([libnotify],[libnotify])
AC_SUBST(libnotify_CFLAGS)
AC_SUBST(libnotify_LIBS)


# Instantiation macros
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
