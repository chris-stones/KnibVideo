#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(knib_read, 0.1, chris.stones@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/knib_read.c)
AC_CONFIG_HEADERS(src/config.h)

AC_CONFIG_MACRO_DIR([m4])

LT_INIT
AM_INIT_AUTOMAKE

#dnl RASPBERY PI - Try to help the user as much as possible! 
#AC_ARG_WITH(
#  [raspberrypi],
#  AS_HELP_STRING([--with-raspberrypi=SYSROOT_DIR], [Target RaspberryPi.]),
#  [
#    RPI_SYSROOT="$withval"
#    CPPFLAGS="$CPPFLAGS -I$RPI_SYSROOT/opt/vc/include"
#    LDFLAGS="$LDFLAGS -L$RPI_SYSROOT/opt/vc/lib -Wl,--rpath-link,$RPI_SYSROOT/opt/vc/lib,--rpath,/opt/vc/lib"
#  ],
#  []
#)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
