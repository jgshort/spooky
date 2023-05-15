m4_define([spooky_major_version],[0])
m4_define([spooky_minor_version],[1])
m4_define([spooky_micro_version],[0]) # Keep this in sync with spooky_crate/Cargo.toml
m4_define([spooky_version],[spooky_major_version.spooky_minor_version.spooky_micro_version])

AC_INIT([spooky], m4_esyscmd_s(echo $(git describe)$(git status --porcelain | awk '{if ($1 == "M") {print "-wip";exit}}')), [53501+jgshort@users.noreply.github.com])

AC_CANONICAL_HOST

WINDOWS_FLAGS=""
OSX_FRAMEWORKS=""
case "${host_os}" in
	cygwin*|mingw*)
		WINDOWS_FLAGS="-lmingw32 -lSDL2main"
		;;
  darwin*)
    # Required by SDL2 Rust library
    OSX_FRAMEWORKS="-framework Security"
    ;;
esac

AC_CONFIG_MACRO_DIR([autoconf-archive/m4])
AC_ENABLE_SHARED
AC_DISABLE_STATIC
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AC_PROG_CC_STDC

AM_PROG_AR
LT_INIT

AC_SUBST([AM_CFLAGS], ["-std=c17 -v -g -O -Wextra -Wfloat-equal -Wunused -Wall -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wunreachable-code -Wno-unused-function -Wno-cpp -Werror -pedantic -pedantic-errors -Waggregate-return -Wcast-qual -Wchar-subscripts -Wcomment -Wconversion -Wdisabled-optimization -Wformat -Wformat=2 -Wformat-nonliteral -Wformat-security -Wformat-y2k -Wimplicit -Wimport -Winit-self -Winline -Winvalid-pch -Wmissing-braces -Wmissing-field-initializers -Wmissing-format-attribute -Wmissing-include-dirs -Wmissing-noreturn -Wpacked -Wpadded -Wparentheses -Wredundant-decls -Wreturn-type -Wsequence-point -Wsign-compare -Wstack-protector -Wstrict-aliasing -Wstrict-aliasing=2 -Wswitch -Wswitch-default -Wswitch-enum -Wtrigraphs -Wuninitialized -Wunknown-pragmas -Wunreachable-code -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value -Wunused-variable -Wvariadic-macros -Wvolatile-register-var -Wwrite-strings ${WINDOWS_FLAGS}"])

SPOOKY_API_VERSION=0.1.0
SPOOKY_API_MAJOR_VERSION=0
SPOOKY_API_MINOR_VERSION=1

AC_SUBST([SPOOKY_API_VERSION])
AC_SUBST([SPOOKY_API_MAJOR_VERSION])
AC_SUBST([SPOOKY_API_MINOR_VERSION])
AC_SUBST([SPOOKY_API_VERSION_U],[AS_TR_SH([$SPOOKY_API_VERSION])])

# ===========================================================================

AC_SUBST([SPOOKY_LT_VERSION_INFO],[spooky_lt_version_info])
AC_SUBST([LIBSPOOKY_MAJOR_VERSION],[spooky_major_version])
AC_SUBST([LIBSPOOKY_MINOR_VERSION],[spooky_minor_version])
AC_SUBST([LIBSPOOKY_MICRO_VERSION],[spooky_micro_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 ])

AC_CHECK_HEADERS([sodium.h],
  [mypj_found_int_headers=yes; break;])
AS_IF([test "x$mypj_found_int_headers" != "xyes"],
  [AC_MSG_ERROR([Unable to find the libsodium headers])])

AC_CHECK_TYPES(long long)

AC_SEARCH_LIBS([sqlite3_open], [sqlite3], [], [
                AC_MSG_ERROR([unable to find the sqlite3_open() function in libsqlite3])
                ])
AC_SEARCH_LIBS([SDL_Init], [SDL2], [], [
                AC_MSG_ERROR([unable to find the SDL_Init() function in libSDL2])
                ])

AC_SEARCH_LIBS([sodium_init], [sodium], [], [
                AC_MSG_ERROR([unable to find the sodium_init() function in libsodium])
                ])
AC_SEARCH_LIBS([curl_easy_init], [curl], [], [
                AC_MSG_ERROR([unable to find the curl_easy_init() function in libcurl])
                ])
AC_SEARCH_LIBS([IMG_Init], [SDL2_image], [], [
                AC_MSG_ERROR([unable to find the IMG_Init() function in libSDL2_image])
                ])
AC_SEARCH_LIBS([TTF_Init], [SDL2_ttf], [], [
                AC_MSG_ERROR([unable to find the TTF_Init() function in libSDL2_ttf])
                ])

AC_SEARCH_LIBS([fmaxf], [m], [], [
                AC_MSG_ERROR([unable to find the fmaxf() function in libm])
                ])

AC_SEARCH_LIBS([deflate, deflateInit, deflateEnd], [z], [], [
                AC_MSG_ERROR([unable to find the deflate() function in zlib])
                ])

#AC_CHECK_PROG(CARGO, [cargo], [yes], [no])
#AS_IF(test x$CARGO = xno,
#    AC_MSG_ERROR([cargo is required.  Please install the Rust toolchain from https://www.rust-lang.org/])
#)
#AC_CHECK_PROG(RUSTC, [rustc], [yes], [no])
#AS_IF(test x$RUSTC = xno,
#    AC_MSG_ERROR([rustc is required.  Please install the Rust toolchain from https://www.rust-lang.org/])
#)

dnl Specify --enable-debug to make a development release.  By default,
dnl we build in public release mode.

#AC_ARG_ENABLE(debug,
#              AC_HELP_STRING([--enable-debug],
#                             [Build Rust code with debugging information [default=no]]),
#              [debug_release=$enableval],
#              [debug_release=no])

#AC_MSG_CHECKING(whether to build Rust code with debugging information)
#if test "x$debug_release" = "xyes" ; then
#    AC_MSG_RESULT(yes)
#    RUST_TARGET_SUBDIR=debug
#else
#    AC_MSG_RESULT(no)
#    RUST_TARGET_SUBDIR=release
#fi
#AM_CONDITIONAL([DEBUG_RELEASE], [test "x$debug_release" = "xyes"])

AC_SUBST(OSX_FRAMEWORKS)
#AC_SUBST([RUST_TARGET_SUBDIR])
AC_OUTPUT

