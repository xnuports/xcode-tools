#!/bin/bash

# xcrun-tool is installed at <developer_dir>/usr/bin/, so xcrun is a
# sibling.  Falls back to PATH.
XCRUN="$(cd "$(dirname "$0")" 2>/dev/null && pwd)/xcrun"
[ -x "${XCRUN}" ] || XCRUN=xcrun

if [ `basename ${0}` == "xcrun-tool" ]; then
	echo "xcrun-tool: error: this tool must not be called directly."
	exit 1
fi

TARGET_TRIPLE=`${XCRUN} --show-sdk-target-triple`
TOOL=`${XCRUN} -find ${0/${TARGET_TRIPLE}-/}`

${TOOL} "${@}"

exit ${?}
