#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
version=2.12
archive_name="wpa_supplicant-${version}.tar.gz"
archive_url="https://w1.fi/releases/${archive_name}"
archive_sha256=08e23937e16d0155e55cab2b51f51fbe10d80a1aa91c4e15442645059b737ef6
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/u80211-wpas.XXXXXX")
install_dir="$script_dir/wpa_install"
stage_dir="$script_dir/.wpa_install.new.$$"
old_dir="$script_dir/.wpa_install.old.$$"
cleanup() {
	status=$?
	rm -rf "$work_dir" "$stage_dir"
	if [ -d "$old_dir" ]; then
		if [ ! -e "$install_dir" ]; then
			mv "$old_dir" "$install_dir"
		else
			rm -rf "$old_dir"
		fi
	fi
	exit "$status"
}
trap cleanup EXIT HUP INT TERM

archive="$work_dir/$archive_name"
if [ -n "${WPASUP_TARBALL:-}" ]; then
	cp "$WPASUP_TARBALL" "$archive"
elif command -v curl >/dev/null 2>&1; then
	 curl --fail --location --retry 3 --output "$archive" "$archive_url"
elif command -v wget >/dev/null 2>&1; then
	 wget --https-only --tries=3 --output-document="$archive" "$archive_url"
else
	echo "build_wpasup.sh: curl or wget is required" >&2
	exit 1
fi

printf '%s  %s\n' "$archive_sha256" "$archive" | sha256sum --check --status
tar -xzf "$archive" -C "$work_dir"
source_dir="$work_dir/wpa_supplicant-${version}"
patch --batch --forward --fuzz=0 -p1 -d "$source_dir" \
	< "$script_dir/patches/wpa_supplicant-${version}-u80211.patch"

cat > "$source_dir/wpa_supplicant/.config" <<'EOF'
CONFIG_DRIVER_U80211=y
CONFIG_L2_PACKET=linux
CONFIG_CTRL_IFACE=y
CONFIG_TLS=internal
CONFIG_CRYPTO=internal
CONFIG_INTERNAL_LIBTOMMATH=y
CONFIG_NO_RANDOM_POOL=y
EOF

jobs=${JOBS:-}
if [ -z "$jobs" ]; then
	jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
fi
make -C "$source_dir/wpa_supplicant" OUT="$work_dir/build" \
	-j "$jobs"

make -C "$source_dir/wpa_supplicant" OUT="$work_dir/build" \
	DESTDIR="$stage_dir" BINDIR=/sbin install

if [ -e "$install_dir" ]; then
	mv "$install_dir" "$old_dir"
fi
mv "$stage_dir" "$install_dir"
rm -rf "$old_dir"

echo "Installed wpa_supplicant and wpa_cli in $install_dir/sbin"
