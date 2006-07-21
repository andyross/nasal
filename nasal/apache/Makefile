# Simple GNU Makefile for now; mod_nasal isn't integrated into the
# automake stuff yet, and probably won't be.

# Note that the build needs write access to your apache installation,
# which in many installations is owned by root.  It's set here to use
# sudo, but you can pick your own mechanism.
SUDO=sudo

# These are the default names, but Ubuntu (and AFAIK all debian
# derivatives) give them separate names to disambiguate with older
# Apache versions.
APXS=apxs
APACHECTL=apachectl
#APXS=apxs2           # Ubuntu/Debian
#APACHECTL=apache2ctl # Ubuntu/Debian


NASAL_SRCS=bitslib.c code.c codegen.c gc.c hash.c iolib.c lex.c lib.c	\
           mathlib.c misc.c parse.c regexlib.c sqlitelib.c string.c	\
           thread-posix.c thread-win32.c unixlib.c utf8lib.c vector.c

CFLAGS=-I ../src -Wc,-Wall
LIBS=-lsqlite3 -lpcre

all: mod_nasal.c $(NASAL_SRCS)
	$(SUDO) $(APXS) -i -a -c $(CFLAGS) $(LIBS) $^
	$(SUDO) $(APACHECTL) restart

clean:
	$(SUDO) rm -rf $(NASAL_SRCS) mod_nasal.la .libs
	rm -rf *.o *.slo *.lo 

%.c:
	ln -s ../src/$@ $@