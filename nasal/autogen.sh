#!/bin/bash
set -e
aclocal
libtoolize --force
autoheader
autoconf
automake -a
./configure "$@"
