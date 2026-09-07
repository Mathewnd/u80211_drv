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
if ! command -v unshare >/dev/null 2>&1 || ! command -v mount >/dev/null 2>&1; then
	echo "required commands 'unshare' and 'mount' were not found" >&2
	exit 1
fi

namespace=u80211-hardware
if ! ip netns add "$namespace"; then
	echo "could not create network namespace '$namespace'" >&2
	exit 1
fi

cleanup() {
	status=$?
	trap - EXIT INT TERM
	for namespace_pid in $(ip netns pids "$namespace" 2>/dev/null); do
		kill "$namespace_pid" 2>/dev/null || true
	done
	ip netns delete "$namespace" >/dev/null 2>&1 || true
	exit "$status"
}
trap cleanup EXIT INT TERM

ip netns exec "$namespace" ip tuntap add dev tap0 mode tap
ip netns exec "$namespace" ip link set dev tap0 up

if [ "$#" -eq 0 ]; then
	set -- ./build/tests/test_hardware
fi

# Network namespaces do not isolate /run. Give the hardware session its own
# runtime directory so clients such as dhcpcd cannot find and command daemons
# running in the host namespace.
ip netns exec "$namespace" unshare --mount --propagation private -- \
	sh -c 'mount -t tmpfs -o mode=755,nosuid,nodev tmpfs /run && exec "$@"' sh "$@"
