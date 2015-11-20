#!/bin/sh

set -e

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig

autoreconf --force --install --symlink --warnings=all

args="\
--sysconfdir=/etc \
--localstatedir=/var \
--prefix=/usr \
--enable-silent-rules"

if test -z "${NOCONFIGURE}"; then
  set -x
  ./configure CFLAGS='-g -O0' $args "$@"
  make clean
fi
