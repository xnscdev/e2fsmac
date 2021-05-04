m4_include([version.m4])

AC_INIT([e2fsmac], E2FSMAC_VERSION, [xnscdev@gmail.com])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE

AC_ARG_WITH([sdk], AS_HELP_STRING([--with-sdk=SDK], [use SDK for the build]),
[sdk="$withval"], [sdk=macosx])
sdkdir=`xcrun -sdk "$sdk" -show-sdk-path`
if test -z "$sdkdir"; then
    AC_MSG_ERROR([could not determine directory of SDK `$sdk'])
fi
AC_SUBST([sdkdir])

sdkversion=`xcrun -sdk "$sdk" -show-sdk-version`
AC_SUBST([sdkversion])

KEXT_NAME=e2fsmac
AC_SUBST([KEXT_NAME])

KEXT_VERSION=[]E2FSMAC_VERSION
AC_SUBST([KEXT_VERSION])

KEXT_BUILD=1
AC_SUBST([KEXT_BUILD])

KEXT_EXE=$KEXT_NAME
AC_SUBST([KEXT_EXE])

KEXT_BUNDLEID=org.xnsc.kext.e2fsmac
AC_SUBST([KEXT_BUNDLEID])

SYS_BUILD=`sw_vers -buildVersion`
AC_SUBST([SYS_BUILD])

CC="`xcrun -find -sdk "$sdk" cc` -isysroot $sdkdir"
AC_PROG_CC([clang cc])
AC_PROG_RANLIB
AC_PROG_SED
AC_PROG_AWK

AC_PATH_PROG([KEXTLIBS], [kextlibs], [no], [/sbin:$PATH])
if test "x$kextlibs" = xno; then
    AC_MSG_ERROR([`kextlibs' not found in PATH])
fi
AC_SUBST([KEXTLIBS])

AC_ARG_WITH([kextdir], AS_HELP_STRING([--with-kextdir=DIR],
[install kext in DIR]),
[case "$withval" in
    /* )
	kextdir="$withval"
	;;
    * )
	AC_MSG_ERROR([--with-kextdir requires an absolute path argument])
	;;
esac], [kextdir=/Library/Extensions])
AC_SUBST([kextdir])

AC_CONFIG_FILES([Makefile src/Info.plist src/Makefile])
AC_OUTPUT
