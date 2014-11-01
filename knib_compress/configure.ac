#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(knib_compress, 0.1, chris.stones@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/main.cpp)
AC_CONFIG_HEADERS(src/config.h)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_SEARCH_LIBS([imgAllocAndRead],[img],[],
  AC_MSG_ERROR([Unable to find img library (libimg.so)]))
  
AC_SEARCH_LIBS([imguCopyImage],[imgutil],[],
  AC_MSG_ERROR([Unable to find img library (libimgutil.so)]))
    
AC_CHECK_HEADERS([libimg.h],[],[AC_MSG_ERROR([Missing libimg.h])])
AC_CHECK_HEADERS([libimgutil.h],[],[AC_MSG_ERROR([Missing libimg.h])])
AC_CHECK_HEADERS([knib_read.h],[],[AC_MSG_ERROR([Missing knib_read.h])])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
