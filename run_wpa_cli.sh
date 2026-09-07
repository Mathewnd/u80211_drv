#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
wpa_cli="$script_dir/wpa_install/sbin/wpa_cli"

if [ ! -x "$wpa_cli" ]; then
	echo "run_wpa_cli.sh: wpa_cli is not installed; run ./build_wpasup.sh first" >&2
	exit 1
fi

exec "$wpa_cli" -p /tmp/wpa_supplicant -i tap0 "$@"
