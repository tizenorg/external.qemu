AC_PREREQ(2.61)
AC_INIT(emulator, 1.0)

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_CXX

AC_CHECK_PROGS(PKG_CONFIG, pkg-config, false)
AC_CHECK_PROGS(AR, ar, false)
AC_PREFIX_DEFAULT(`pwd`)

if test "$PKG_CONFIG" = "false"
then
	AC_MSG_ERROR([pkg-config not present, please install])
fi

# CFLAGS="-g -Wall -D_FORTIFY_SOURCE=1 -Wl,--export-dynamic"
CFLAGS="-g -Wall -D_FORTIFY_SOURCE=1"

AC_SUBST(OS_FLAGS)
AC_SUBST(TARGETOS)
AC_SUBST(DEF)	
TARGETOS=`uname -s`
if test "$TARGETOS" = "Linux" 
then
	OS_FLAGS=""
	CC+=" $OS_FLAGS" 
	DEF+="_XOPEN_SOURCE=500"
	DEF+=" _BSD_SOURCE"
elif test "$TARGETOS" = "CYGWIN*"
then
	OS_FLAGS="-mno-cygwin -mwindows -mconsole"
	CC+=" $OS_FLAGS" 
	DEF+=""
else
	OS_FLAGS="-mwindows -mconsole"
	CC+=" $OS_FLAGS" 
	DEF+=""	
fi

# FIXME: might be a better way to do this using debian/rules
deb_changelog="../debian/changelog"
if test -f "$deb_changelog" ; then
	DEBIAN_VERSION=`head -1 "$deb_changelog" | cut -f2 -d\( | cut -f1 -d\)`
else
	DEBIAN_VERSION="$PACKAGE_VERSION"
fi
AC_SUBST(DEBIAN_VERSION)


AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
GLIB_CFLAGS=`$PKG_CONFIG --cflags glib-2.0`
GLIB_LIBS=`$PKG_CONFIG --libs glib-2.0`

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
GTK_CFLAGS=`$PKG_CONFIG --cflags gtk+-2.0`
GTK_LIBS=`$PKG_CONFIG --libs gtk+-2.0`

AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)
GMODULE_CFLAGS=`$PKG_CONFIG --cflags gmodule-2.0`
GMODULE_LIBS=`$PKG_CONFIG --libs gmodule-2.0`

AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)
XML_CFLAGS=`$PKG_CONFIG --cflags libxml-2.0`
XML_LIBS=`$PKG_CONFIG --libs libxml-2.0`

AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)
GTHREAD_CFLAGS=`$PKG_CONFIG --cflags gthread-2.0`
GTHREAD_LIBS=`$PKG_CONFIG --libs gthread-2.0`

if test "$TARGETOS" = "Linux" 
then
	AC_SUBST(ALSA_CFLAGS)
	AC_SUBST(ALSA_LIBS)
	ALSA_CFLAGS=`$PKG_CONFIG --cflags alsa`
	ALSA_LIBS=`$PKG_CONFIG --libs alsa`
fi

saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $GTK_CFLAGS -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <gtk.h>]], [[]])],
		[noisy_gtk_headers=no],[noisy_gtk_headers=yes])
CFLAGS="$saved_CFLAGS"

if test "$noisy_gtk_headers" = "yes"
then
	GTK_CFLAGS="$GTK_CFLAGS -DGTK_DISABLE_DEPRECATED"
fi

AC_CONFIG_FILES([
	Makefile
	src/Makefile])

(cd ../ && ./qemu_configure.sh) || exit 1

AC_OUTPUT
