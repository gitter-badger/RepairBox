#!/bin/bash
if [ ! -d python ]; then
  tar -xf python.tar.gz
  pushd python
  make clean
  ./configure --build=i686-pc-linux-gnu --with-dbmliborder=gdbm --with-threads "CFLAGS=-m32 -std=gnu99 -fgnu89-inline" "CXXFLAGS=-m32" "LDFLAGS=-m32"
  popd
fi

if [ ! -d test ]; then
  tar -xf tests.tar.gz
  mv test python
fi