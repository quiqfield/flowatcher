#!/bin/bash

BASE_DIR=`dirname "${BASH_SOURCE[0]}"`

cd ${BASE_DIR}
BASE_DIR=`pwd`
git submodule update --init

echo "---------------------- Build DPDK ----------------------"
cd deps/dpdk
make -j 8 install T=x86_64-native-linuxapp-gcc

echo "------------------ Bulid libwebsockets -----------------"
cd ../libwebsockets
mkdir build target
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=../target ..
make -j 8
make install

echo "------------------- Build Flowatcher -------------------"
cd ${BASE_DIR}
export RTE_SDK=${BASE_DIR}/deps/dpdk
make clean
make -j 8
