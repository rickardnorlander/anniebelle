AC_PREREQ([2.69])
AC_INIT(anniebelle, 0.1.0)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/anniebelle.cc])
AC_CONFIG_HEADERS(config.h)
AC_PROG_CXX

AM_PATH_GTK_3_0([3.0.0],,AC_MSG_ERROR([Gtk+ 3.0.0 or higher required.]))

AC_CHECK_HEADERS([X11/XKBlib.h])
AC_CHECK_HEADERS([getopt.h])

AC_CHECK_LIB([X11], [XkbOpenDisplay], [],[
         echo "Xkb library is needed to detect bells"
         exit -1])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
