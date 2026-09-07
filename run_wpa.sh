#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
supplicant="$script_dir/wpa_install/sbin/wpa_supplicant"
config="$script_dir/tests/wpa_supplicant.conf"

if [ ! -x "$supplicant" ]; then
	echo "run_wpa.sh: wpa_supplicant is not installed; run ./build_wpasup.sh first" >&2
	exit 1
fi

if [ ! -S /tmp/.u80211_sock ]; then
	echo "run_wpa.sh: /tmp/.u80211_sock is unavailable; run this from the test_hardware shell" >&2
	exit 1
fi

exec "$supplicant" -B -D u80211 -i tap0 -c "$config" "$@"
