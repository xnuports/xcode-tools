#!/bin/sh
#
# Build xnu's generated headers with tools/darwin-xnu-build.
#
# Much of mach/ is mig's output rather than a file anyone wrote, and
# Kernel.framework's headers are assembled by xnu's own build.  Both
# come out of darwin-xnu-build's fakeroot, which sdk-headers then
# takes from.
#
# Two things stand between that script and a working run here, and
# this deals with both:
#
#   * It names one exact Kernel Debug Kit per release -- for 26.5,
#     KDK_26.5_25F71 -- and downloads and installs another when that
#     precise build is not present, which wants sudo.  A machine
#     patched past the named build (this one has KDK_26.5.2_25F84)
#     therefore never uses the KDK it already has.  Whichever KDK
#     matching the release is installed is used instead.
#
#   * A Vulkan SDK puts a *directory* named cmake on PATH, which
#     shadows the real one and makes AvailabilityVersions fail to
#     configure with a bare "Error 126".
#
# The submodule is left untouched: the adjustments are made to a copy.
#
# Usage: xnu-headers.sh [-f]     -f rebuilds an existing fakeroot.
#
set -e

TOP=$(cd "$(dirname "$0")/../.." && pwd)
XNUB="${TOP}/tools/darwin-xnu-build"
MACOS_VERSION="${MACOS_VERSION:-26.5}"
FORCE=0
[ "${1:-}" = "-f" ] && FORCE=1

[ -f "${XNUB}/build.sh" ] || {
	echo "xnu-headers: ${XNUB} is empty; git submodule update --init" >&2
	exit 1
}

# Producing the fakeroot wants the network and the better part of an
# hour, so having one already is the common case.
if [ "${FORCE}" -eq 0 ] &&
   [ -f "${XNUB}/fakeroot/usr/include/mach/clock.h" ]; then
	echo "xnu-headers: fakeroot is already built (-f to rebuild)"
	exit 0
fi

# The KDK this machine has, rather than the one the script names.
# Sorting takes the newest when several are installed.
KDK=$(ls -d "/Library/Developer/KDKs/KDK_${MACOS_VERSION}"*.kdk 2>/dev/null |
      sort -V | tail -1)
if [ -z "${KDK}" ]; then
	echo "xnu-headers: no KDK_${MACOS_VERSION}*.kdk in /Library/Developer/KDKs." >&2
	echo "  Install a Kernel Debug Kit for macOS ${MACOS_VERSION} from" >&2
	echo "  developer.apple.com; the build reads the kernel's own" >&2
	echo "  headers out of it and cannot proceed without one." >&2
	exit 1
fi
echo "xnu-headers: using ${KDK}"

# A directory named cmake earlier in PATH than the real one is enough
# to stop the build, and says only "Error 126" when it does.
CMAKE=$(command -v cmake 2>/dev/null || true)
if [ -z "${CMAKE}" ] || [ ! -f "${CMAKE}" ]; then
	for d in /opt/homebrew/bin /usr/local/bin; do
		if [ -f "$d/cmake" ] && [ -x "$d/cmake" ]; then
			echo "xnu-headers: cmake on PATH is not a file; using $d/cmake"
			PATH="$d:${PATH}"
			export PATH
			break
		fi
	done
fi

GEN="${TOP}/build/xnu"
mkdir -p "${GEN}"

# Point every KDKROOT at the installed KDK, so choose_xnu's "not
# installed, download it" branch is not taken, and stop before the
# kernel itself: headers are all the SDK needs.  build.sh takes its
# working directory from $PWD, not from where the script itself
# lives, so a copy run from the submodule behaves the same.
sed -E \
    -e "s|^([[:space:]]*)KDKROOT='.*'|\\1KDKROOT='${KDK}'|" \
    -e 's|^main "\$@"$||' \
    "${XNUB}/build.sh" > "${GEN}/build-headers.sh"

cat >> "${GEN}/build-headers.sh" <<'DRIVER'
headers_only() {
    install_deps
    setup_xcode_toolchain
    choose_xnu
    get_xnu
    patches
    venv
    build_bootstrap_cmds
    build_dtrace
    build_availabilityversions
    xnu_headers
    libsystem_headers
    libsyscall_headers
    echo "  🎉 XNU headers done"
}
headers_only
DRIVER

bash -n "${GEN}/build-headers.sh" || {
	echo "xnu-headers: generated script does not parse; build.sh has changed shape" >&2
	exit 1
}

cd "${XNUB}"
MACOS_VERSION="${MACOS_VERSION}" bash "${GEN}/build-headers.sh"
