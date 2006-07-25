bin_PROGRAMS = nasal-bin

lib_LIBRARIES = libnasal.a

include_HEADERS = nasal.h

bin_SCRIPTS = nasal

if HAVE_PCRE
pcre = regexlib.c
endif

if HAVE_SQLITE
sqlite = sqlitelib.c
endif

libnasal_a_SOURCES = bitslib.c code.c codegen.c debug.c gc.c hash.c	\
                     iolib.c lex.c lib.c mathlib.c misc.c parse.c	\
                     string.c thread-posix.c thread-win32.c unixlib.c	\
                     utf8lib.c vector.c code.h data.h iolib.h nasal.h	\
                     parse.h $(pcre) $(sqlite)

nasal_bin_SOURCES = nasal-bin.c

nasal_bin_LDADD = libnasal.a


