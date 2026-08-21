#!/bin/bash

XCRUN=/opt/xnuports/usr/bin/xcrun

TOOL=`${XCRUN} -find clang++`

${TOOL} "${@}"

exit ${?}
