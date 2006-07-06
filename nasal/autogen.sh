#!/bin/bash
(cd src; ln -s ../../*.[ch])
touch config.h.in
aclocal
autoconf
automake -a
./configure
make dist
make distclean

