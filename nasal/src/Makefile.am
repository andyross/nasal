bin_PROGRAMS = nasal-bin

if HAVE_PCRE
pcre = regexlib.c
endif

if HAVE_SQLITE
sqlite = sqlitelib.c
endif

nasal_bin_SOURCES = nasal-bin.c bitslib.c code.c codegen.c debug.c	\
                    gc.c hash.c iolib.c lex.c lib.c mathlib.c misc.c	\
                    parse.c string.c thread-posix.c thread-win32.c	\
                    unixlib.c utf8lib.c vector.c code.h data.h iolib.h	\
                    nasal.h parse.h $(pcre) $(sqlite)


