# Flowatcher

Flowatcher is a real-time flow-based traffic monitoring tool for 10 Gbit Ethernet.
This is built on DPDK (http://dpdk.org/), and based in part on the work of the libwebsockets project (https://libwebsockets.org/).


# Installation

1. Install the dependencies (see below)
2. git submodule update --init
3. ./build.sh
4. ./setup-dpdk.sh
5. ./run.sh (Run Flowatcher App)

Note: The scripts work on x86_64 Linux.

## Requirements
* NICs supported by DPDK
* 16GB memory

## Dependencies
* gcc
* make
* cmake (2.8 or greater)
* openssl (for building libwebsockets)
* kernel headers (for the DPDK igb-uio driver)

for Debian/Ubuntu:
```
# apt-get install gcc make cmake openssl linux-headers-$(uname -r)
```

