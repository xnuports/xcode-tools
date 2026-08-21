#!/bin/bash

XCRUN=/opt/xnuports/bin/xcrun

TOOL=`${XCRUN} -find clang`

${TOOL} "${@}"

exit ${?}
