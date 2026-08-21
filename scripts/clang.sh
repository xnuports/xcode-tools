#!/bin/bash

XCRUN=/opt/xnuports/usr/bin/xcrun

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
