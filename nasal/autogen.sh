#!/bin/bash
set -e
touch config.h.in
aclocal
autoconf
automake -a
./configure
