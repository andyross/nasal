#!/bin/bash
set -e
aclocal
autoheader
autoconf
automake -a
./configure "$@"
