#!/bin/bash
set -e
touch config.h.in
aclocal
autoheader
autoconf
automake -a
./configure
