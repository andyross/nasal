# Simple GNU Makefile for now; mod_nasal isn't integrated into the
# automake stuff yet, and probably won't be.

# Note that the build needs write access to your apache installation,
# which in many installations is owned by root.  It's set here to use
# sudo, but you can pick your own mechanism.
SUDO=sudo

NASAL_OBJS=bitslib.o code.o codegen.o gc.o hash.o iolib.o lex.o lib.o	\
           mathlib.o misc.o parse.o regexlib.o string.o thread-posix.o	\
           thread-win32.o unixlib.o utf8lib.o vector.o

all: mod_nasal.c libnasal.a
	$(SUDO) apxs -i -a -c -I ../src -Wc,-Wall mod_nasal.c libnasal.a
	$(SUDO) apachectl restart

clean:
	$(SUDO) rm -rf libnasal.a $(NASAL_OBJS) mod_nasal.*o mod_nasal.la .libs

libnasal.a: $(NASAL_OBJS)
	ar cr $@ $^

%.o : ../src/%.c
	gcc -c -Wall -O2 -g -fPIC -pthread -D_REENTRANT $< 