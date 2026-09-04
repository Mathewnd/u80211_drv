#!/bin/sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
	echo "run this script with root privileges" >&2
	exit 1
fi

if ! command -v ip >/dev/null 2>&1; then
	echo "required command 'ip' was not found" >&2
	exit 1
fi

if [ -e /sys/class/net/tap0 ]; then
	echo "'tap0' already exists" >&2
	exit 1
fi

ip tuntap add dev tap0 mode tap
ip link set dev tap0 up

echo "created tap0"
