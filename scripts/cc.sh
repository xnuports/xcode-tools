#!/bin/bash

# Resolve xcrun relative to our own location: these shims are installed
# at <developer_dir>/Toolchains/<name>.xctoolchain/usr/bin/, so the
# Developer directory is four levels up.  Falls back to PATH.
XCRUN="$(cd "$(dirname "$0")/../../../../usr/bin" 2>/dev/null && pwd)/xcrun"
[ -x "${XCRUN}" ] || XCRUN=xcrun

TOOL=`${XCRUN} -find clang`

${TOOL} "${@}"

exit ${?}
