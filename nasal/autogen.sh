#!/bin/bash
set -e
(cd src; ln -fs ../../*.[ch] .)
touch config.h.in
aclocal
autoconf
automake -a
./configure
make
make dist
make distclean

