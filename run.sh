#!/bin/bash

if [[ ${UID} -ne 0 ]]; then
	echo "This script must be run as root."
	exit 1
fi

BASE_DIR=`dirname "${BASH_SOURCE[0]}"`
cd ${BASE_DIR}
BASE_DIR=`pwd`
export RTE_SDK=${BASE_DIR}/deps/dpdk
export RTE_TARGET="x86_64-native-linuxapp-gcc"

make run
