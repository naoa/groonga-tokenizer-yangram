AC_PREREQ(2.59)
AC_INIT([groonga], 1.0.1, [naoya@createfield.com])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([tokenizers/yangram.c])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE(foreign)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC

AC_PROG_LIBTOOL

if test "$GCC" = "yes"; then
  CFLAGS="$CFLAGS -Wall -Wextra"
  CFLAGS="$CFLAGS -Wmissing-declarations -Wmissing-prototypes"
fi

AC_ARG_ENABLE(debug,
  [AS_HELP_STRING([--enable-debug],
                  [use debug flags (default=no)])],
  [sample_debug="$enableval"],
  [sample_debug="no"])
if test "x$sample_debug" != "xno"; then
  if test "$CLANG" = "yes"; then
    CFLAGS="$CFLAGS -O0 -g"
  elif test "$GCC" = "yes"; then
    CFLAGS="$CFLAGS -O0 -g3"
  fi
fi

GROONGA_REQUIRED_VERSION=4.0.5
PKG_CHECK_MODULES([GROONGA], [groonga >= ${GROONGA_REQUIRED_VERSION}])

_PKG_CONFIG(GROONGA_PLUGINS_DIR, [variable=pluginsdir],    [groonga])
_PKG_CONFIG(GROONGA,             [variable=groonga],       [groonga])

GROONGA_PLUGINS_DIR="${pkg_cv_GROONGA_PLUGINS_DIR}"
GROONGA="${pkg_cv_GROONGA}"

AC_SUBST(GROONGA_PLUGINS_DIR)
AC_SUBST(GROONGA)

tokenizers_pluginsdir="\${GROONGA_PLUGINS_DIR}/tokenizers"
AC_SUBST(tokenizers_pluginsdir)

AC_CONFIG_FILES([
  Makefile
  groonga-tokenizer-yangram.pc
  tokenizers/Makefile
  test/Makefile
  packages/Makefile
  packages/apt/Makefile
  packages/yum/Makefile
  packages/rpm/Makefile
  packages/rpm/centos/Makefile
  packages/rpm/centos/groonga-tokenizer-yangram.spec
  packages/rpm/fedora/Makefile
  packages/rpm/fedora/groonga-tokenizer-yangram.spec
  packages/source/Makefile
])

AC_OUTPUT

echo "$PACKAGE_NAME $PACKAGE_VERSION configuration:"
echo "-----------------------"
echo "  Compiler:              ${CC}"
echo "  CFLAGS:                ${CFLAGS}"
echo "  CXXFLAGS:              ${CXXFLAGS}"
echo "  Libraries:             ${LIBS}"
echo
echo "groonga"
echo "  CFLAGS:                ${GROONGA_CFLAGS}"
echo "  Libraries:             ${GROONGA_LIBS}"
echo "  install directory:     ${tokenizers_pluginsdir}"
echo

