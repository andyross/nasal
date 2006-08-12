bin_PROGRAMS = nasal-bin

lib_LTLIBRARIES = libnasal.la

include_HEADERS = nasal.h naref.h

dist_bin_SCRIPTS = nasal

if HAVE_PCRE
pcre = regexlib.c
endif

if HAVE_SQLITE
sqlite = sqlitelib.c
endif

if HAVE_READLINE
readline = readlinelib.c
endif

libnasal_la_SOURCES = bitslib.c code.c codegen.c debug.c gc.c hash.c	\
                      iolib.c lex.c lib.c mathlib.c misc.c parse.c	\
                      string.c thread-posix.c thread-win32.c		\
                      threadlib.c unixlib.c utf8lib.c vector.c code.h	\
                      data.h iolib.h nasal.h parse.h $(pcre) $(sqlite)	\
                      $(readline)

libnasal_la_LDFLAGS = -version-info @LIBTOOL_VERSION_INFO@

nasal_bin_SOURCES = nasal-bin.c

nasal_bin_LDADD = .libs/libnasal.a


