# Author: geaaru

AC_INIT(sql_ocioracle.c)
AC_REVISION($Revision: 1.10 $)
AC_DEFUN(modname,[rlm_sql_ocioracle])

AC_ARG_WITH(oracle_lib_dir, [AC_HELP_STRING([--with-oracle-lib-dir],
                                    [Path of the Oracle OCI libraries])],
            [ oracle_lib_dir="$withval" ], [ oracle_lib_dir="" ])
AC_ARG_WITH(oracle_include_dir, [AC_HELP_STRING([--with-oracle-include-dir],
                                    [Path of the Oracle OCI includes])],
            [ oracle_include_dir="$withval" ], [ oracle_include_dir="" ])
AC_ARG_WITH(ocilib_lib_dir, [AC_HELP_STRING([--with-ocilib-lib-dir],
                                      [Path of the opensource ocilib library])],
            [ ocilib_lib_dir="$withval" ], [ ocilib_lib_dir="" ])
AC_ARG_WITH(ocilib_include_dir, [AC_HELP_STRING([--with-ocilib-include-dir],
                                      [path of the opensource ocilib library includes])],
            [ ocilib_include_dir="$withval" ], [ ocilib_include_dir="" ])
AC_ARG_WITH(oracle_version, [AC_HELP_STRING([--with-oracle-version],
                                            [Version of Oracle libraries. Possible values are 9,10,11 or 12])],
            [oracle_version="$withval" ], [ oracle_version="9 10 11 12" ])

fail=
sql_ocioracle_ldflags=
sql_ocioracle_cflags=
OCI_INCLUDES=
OOCILIB_INCLUDES=
OCILIB=

dnl ######################################################
dnl # Check for header files of Oracle Libraries
dnl ######################################################

CFLAGS_PRE="$CFLAGS"
if test x$oracle_include_dir != x ; then
  OCI_INCLUDES="-I$oracle_include_dir"
else
  if text x"$ORACLE_HOME" != "x" ; then
    OCI_INCLUDES="-I${ORACLE_HOME}/include"
  fi
fi
CFLAGS="$CFLAGS $OCI_INCLUDES"

AC_CHECK_HEADER([oci.h],
                [],
                [AC_MSG_ERROR([Oracle headers not found. Use --with-oracle-include-dir=<path> or set ORACLE_HOME.])],
                [#include <oci.h>
                ])
CFLAGS="$CFLAGS_PRE"
AC_SUBST(OCI_INCLUDES)

dnl ######################################################
dnl # Check for header files of Opensource OCI Library
dnl ######################################################

CFLAGS_PRE="$CFLAGS"
if test x$ocilib_include_dir != x ; then
    OOCILIB_INCLUDES="-I$ocilib_include_dir"
    CFLAGS="$CFLAGS $OOCILIB_INCLUDES"
fi
AC_CHECK_HEADER([ocilib.h],
                [],
                [AC_MSG_ERROR([Opensource Ocilib headers not found. Use --with-ocilib-include-dir=<path>.])],
                [#include <ocilib.h>
                ])
CFLAGS="$CFLAGS_PRE"
AC_SUBST(OOCILIB_INCLUDES)

dnl ######################################################
dnl # Check for libraries of Oracle Libraries
dnl ######################################################

LDFLAGS_PRE="$LDFLAGS"
OCILIB_PATH=""
if test x$oracle_lib_dir != x ; then
  OCILIB_PATH="$oracle_lib_dir"
else
  if text x"$ORACLE_HOME" != "x" ; then
    OCILIB_PATH="${ORACLE_HOME}/lib"
  fi
fi

for version in $oracle_version ; do

  LDFLAGS="-L${OCILIB_PATH} -lnnz${version} -L${OCILIB_PATH} -locci -L${OCILIB_PATH} -lclntsh"

  # Avoid caching of result and permit check
  # of the same library for every oracle version.
  unset ac_cv_lib_clntsh__OCIInitialize_

  AC_CHECK_LIB(clntsh, [ OCIInitialize ],
               ocilib_present=1,
               ocilib_present=0)

  if test $ocilib_present -eq 1 ; then
    OCILIB="-L${OCILIB_PATH} -lnnz${version} -L${OCILIB_PATH} -locci -L${OCILIB_PATH} -lclntsh"
    break
  fi
done

if test $ocilib_present -eq 0 ; then

  AC_MSG_ERROR([Oracle libraries not found. Use --with-oracle-lib-dir=<path> or set ORACLE_HOME.])

fi
LDFLAGS="$LDFLAGS_PRE"
AC_SUBST(OCILIB)

dnl ######################################################
dnl # Check for libraries of Opensource OCIlib
dnl ######################################################


# Check for Opensource Ocilib library
LDFLAGS_PRE="$LDFLAGS"
if test x$ociliblib != x ; then
    OOCILIB="-L$ociliblib"
    LDFLAGS="$LDFLAGS -L$ociliblib $OCILIB"
else
    LDFLAGS="$LDFLAGS $OCILIB"
fi

AC_CHECK_LIB(ocilib, [ OCI_ConnectionCreate ],
             oocilib_present=1,
             oocilib_present=0)

if test $oocilib_present -eq 1 ; then
  OOCILIB="${OOCILIB} -locilib"
  LDFLAGS="$LDFLAGS_PRE"
else
  AC_MSG_ERROR([Opensource Ocilib library not found. Use --with-ocilib-lib-dir=<path>.])
fi
AC_SUBST(OOCILIB)


sql_ocioracle_cflags="$OOCILIB_INCLUDES $OCI_INCLUDES"
sql_ocioracle_ldflags="$OOCILIB $OCILIB"
targetname=modname

dnl Don't change this section.
if test "x$fail" != x; then
  if test "x${enable_strict_dependencies}" = xyes; then
    AC_MSG_ERROR([set --without-]modname[ to disable it explicitly.])
  else
    AC_MSG_WARN([silently not building ]modname[.])
    AC_MSG_WARN([FAILURE: ]modname[ requires:$fail.]);
    targetname=
  fi
fi

AC_SUBST(sql_ocioracle_ldflags)
AC_SUBST(sql_ocioracle_cflags)
AC_SUBST(targetname)
AC_OUTPUT(all.mk)
