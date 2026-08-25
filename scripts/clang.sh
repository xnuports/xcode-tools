#!/bin/bash

# Resolve xcrun relative to our own location: these shims are installed
# at <developer_dir>/Toolchains/<name>.xctoolchain/usr/bin/, so the
# Developer directory is four levels up.  Falls back to PATH.
XCRUN="$(cd "$(dirname "$0")/../../../../usr/bin" 2>/dev/null && pwd)/xcrun"
[ -x "${XCRUN}" ] || XCRUN=xcrun

##
# clang/clang++ wrapper for cross-compiling.
# This script is invoked by xcrun upon calling clang.
##

TOOL=`${XCRUN} -sdk / -find ${0}`
SDKROOT=`${XCRUN} --show-sdk-path`
TARGET_TRIPLE=`${XCRUN} --show-sdk-target-triple`
TOOLCHAIN_DIR=`${XCRUN} --show-sdk-toolchain-path`

${TOOL} -target ${TARGET_TRIPLE} -isysroot ${SDKROOT} -B${TOOLCHAIN_DIR}/usr/bin "${@}"

exit ${?}
