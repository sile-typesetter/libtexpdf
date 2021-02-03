dnl Process this file with autoconf to produce a configure script.
dnl
dnl   Copyright (C) 2009-2014 Peter Breitenlohner <tex-live@tug.org>
dnl
dnl   This file is free software; the copyright holder
dnl   gives unlimited permission to copy and/or distribute it,
dnl   with or without modifications, as long as this notice is preserved.
dnl
AC_INIT([libtexpdf],[20140927],[tex-k@tug.org])
AC_PREREQ([2.70])
AC_CONFIG_SRCDIR([pdfdoc.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT()

KPSE_COMMON([libtexpdf])

AC_CONFIG_HEADERS([config.h])

AM_SILENT_RULES([yes])

dnl We're not using kpse, but just using its m4 system to aid
dnl integration into the TL tree

dnl Checks for header files.
AC_CHECK_HEADERS([unistd.h stdint.h inttypes.h sys/types.h sys/wait.h stdbool.h])

dnl Checks for library functions.
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([open close getenv basename])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_STRUCT_TM
AC_STRUCT_TIMEZONE
AC_C_BIGENDIAN
AC_TYPE_INT32_T
AC_TYPE_UINT32_T

AC_CHECK_DECL([timezone],
              [AC_DEFINE([HAVE_TIMEZONE], 1,
                         [Define if <time.h> has timezone as an external variable.])],
              [],
              [[#include <time.h>]])
AC_CHECK_MEMBER([struct tm.tm_gmtoff],
                [AC_DEFINE([HAVE_TM_GMTOFF], 1,
                           [Define if struct tm has tm_gmtoff as a member.])],
                [],
                [[#include <time.h>]])

AC_CHECK_SIZEOF([char])
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])

AC_SEARCH_LIBS([pow], [m])

KPSE_ZLIB_FLAGS
PKG_CHECK_MODULES(LIBPNG, libpng,[],[AC_MSG_FAILURE([libpng not available or not configured with pkg-config])])

AC_CHECK_LIB([png],[png_get_image_width],[],[AC_MSG_FAILURE([Suitable libpng not found])],[$LIBPNG_LIBS])

if test "x$enable_build" != xno || test -f config.force; then

KPSE_ADD_FLAGS([zlib])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <zlib.h>]],
                                [[Bytef *dst,*src; uLong dstlen, srclen; int lvl, err;
                                  err=compress2(dst, &dstlen, src, srclen, lvl);]])],
               [],
               [AC_MSG_ERROR([Sorry, you need zlib with compress2])])

echo timestamp >config.force
fi

dnl We require zlib with compress2 and libpng
AC_DEFINE([HAVE_ZLIB], 1, [Define if you have zlib and its headers.])
AC_DEFINE([HAVE_ZLIB_COMPRESS2], 1, [Define if your zlib has the compress2 function.])
AC_DEFINE([HAVE_LIBPNG], 1, [Define if you have libpng and its headers.])

AC_CONFIG_FILES([Makefile ])

AC_OUTPUT
