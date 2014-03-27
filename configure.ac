#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mighty_dma], [1.0], [q@crypto.com])
AC_CONFIG_SRCDIR([src/edma/mighty_dma.c])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile] [src/edma/Makefile])
AC_OUTPUT

