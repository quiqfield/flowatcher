#!/bin/bash

if [[ ${UID} -ne 0 ]]; then
	echo "This script must be run as root."
	exit 1
fi

BASE_DIR=`dirname "${BASH_SOURCE[0]}"`
RTE_TARGET=x86_64-native-linuxapp-gcc

mkdir -p /mnt/huge
(mount | grep hugetlbfs >/dev/null) || mount -t hugetlbfs nodev /mnt/huge
echo 7168 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

cd ${BASE_DIR}/deps/dpdk
modprobe uio
(lsmod | grep igb_uio >/dev/null) || insmod ./${RTE_TARGET}/kmod/igb_uio.ko

i=0
for id in $(tools/dpdk_nic_bind.py --status | grep -v Active | grep "unused=igb_uio" | awk '{ print $1 }')
do
	echo "Binding interface $id to DPDK"
	tools/dpdk_nic_bind.py --bind=igb_uio $id
	i=$(($i+1))
done

if [[ $i -eq 0 ]]; then
	echo "Could not find any available interfaces for DPDK."
fi

tools/dpdk_nic_bind.py --status
