#!/bin/sh

mkdir -p m4

aclocal --install || exit 1
autoreconf --verbose --force --install || exit 1
automake --add-missing || exit 1

