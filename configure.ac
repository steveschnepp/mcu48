
m4_include([version.m4])
AC_INIT([x48],[VERSION_NUMBER],[x48-bugs@gam3.net])

AC_COPYRIGHT([Copyright (c) 2006-2010 G. Allen Morris III])
AC_CANONICAL_TARGET

AC_PREREQ([2.69])

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CPP
AC_USE_SYSTEM_EXTENSIONS

PKG_PROG_PKG_CONFIG([0.20])


# ********* readline library
AC_ARG_ENABLE(readline,
  [  --disable-readline      disable readline support],
  [ if test x"$enableval" = xyes; then
    with_readline="yes, check"
  else
    with_readline="no"
    problem_readline=": Explicitly disabled"
  fi ],
  [ with_readline="not specified, check" ]
)

AH_TEMPLATE([HAVE_READLINE],[Define if Readline is used.])
if test x"$with_readline" != xno; then
  AC_CHECK_LIB(readline, readline,
               with_readline=yes;
	       LIBS="$LIBS -lreadline";
	       AC_DEFINE(HAVE_READLINE),
               with_readline=no;
               problem_readline=": Can't detect readline library.",
               [])
  AC_CHECK_LIB(history, add_history)
  AC_CHECK_HEADERS([readline/readline.h])

  AC_CHECK_HEADERS([readline/history.h])
fi

AC_CHECK_HEADERS([fcntl.h memory.h stdlib.h string.h sys/ioctl.h sys/time.h termios.h unistd.h stdint.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM


AC_DEFINE([VERSION_MAJOR], _VERSION_MAJOR, [The major version number])
AC_DEFINE([VERSION_MINOR], _VERSION_MINOR, [The minor version number])
AC_DEFINE([PATCHLEVEL], _PATCHLEVEL, [The patch level])


AC_DEFINE_UNQUOTED([COMPILE_TIME], "`date`", [Date program was compiled])
AC_DEFINE([COMPILE_VERSION], [0], [description])

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_CHECK_FUNCS([bzero gethostname gettimeofday memset mkdir select strdup strrchr uname])

AM_INIT_AUTOMAKE

AC_CONFIG_FILES([Makefile
                 romdump/Makefile
                 src/x48.man
		 rpm/x48.spec
                 src/Makefile])
AC_OUTPUT

echo "
X48 Configuration:

  Version:     $VERSION$VERSIONINFO

  Executables: $bindir
  Man pages:   $mandir

  With ReadLine sup. in Debuger?  $with_readline$problem_readline

# See INSTALL for the description of what this may mean."

