#!/bin/sh
PREFIX=${PWD}/bin
CONF_OPTS="-DPCH=1 -DDEBUG=1 -DUSE_STD_MALLOC=1 "


CFLAGS="-march=native -O2 -DNDEBUG -pipe -ggdb -fno-strict-aliasing -fno-delete-null-pointer-checks -D_LARGEFILE_SOURCE"
CXXFLAGS="${CFLAGS}"

rm -Rf build &&
mkdir build &&
cd build &&

cmake .. ${CONF_OPTS} \
    -DPREFIX="${PREFIX}" \
    -DCMAKE_C_FLAGS="${CFLAGS}" \
    -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
    -DCMAKE_C_COMPILER="gcc" \
    -DCMAKE_CXX_COMPILER="g++"
