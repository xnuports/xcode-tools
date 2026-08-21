#!/bin/bash

XCRUN=/opt/xnuports/bin/xcrun

if [ `basename ${0}` == "xcrun-tool" ]; then
	echo "xcrun-tool: error: this tool must not be called directly."
	exit 1
fi

TARGET_TRIPLE=`${XCRUN} --show-sdk-target-triple`
TOOL=`${XCRUN} -find ${0/${TARGET_TRIPLE}-/}`

${TOOL} "${@}"

exit ${?}
