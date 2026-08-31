#!/bin/sh
#
# emit-sdksettings.sh -- write an SDKSettings.plist to stdout.
#
# The tree emits more than one SDK bundle and they differ only in who
# they say they are: the internal SDK is the same macOS SDK with
# ".internal" on its canonical name.  Generating them from one
# description keeps the two from drifting, which is the whole risk with
# a second copy of a forty-line plist.
#
# The canonical name is the part that matters.  It is what a build
# system asks for -- `xcrun --sdk macosx.internal` and `-sdk
# macosx26.5.internal` both resolve through it -- so the suffix is not
# decoration, it is the SDK's identity.
#
# Usage:
#	emit-sdksettings.sh <canonical> <display> <version> \
#	                    <deployment-target> <arch>
#
# Copyright (c) 2026 Sunneva N. Mariu
# SPDX-License-Identifier: BSD-3-Clause

set -e

if [ $# -ne 5 ]; then
	echo "usage: $0 <canonical> <display> <version> <deployment> <arch>" >&2
	exit 1
fi

canonical=$1
display=$2
version=$3
deployment=$4
arch=$5

cat <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CanonicalName</key>
	<string>${canonical}</string>
	<key>DisplayName</key>
	<string>${display}</string>
	<key>MinimalDisplayName</key>
	<string>${version}</string>
	<key>Version</key>
	<string>${version}</string>
	<key>IsBaseSDK</key>
	<string>YES</string>
	<key>DefaultDeploymentTarget</key>
	<string>${deployment}</string>
	<key>DefaultProperties</key>
	<dict>
		<key>PLATFORM_NAME</key>
		<string>macosx</string>
		<key>DEFAULT_COMPILER</key>
		<string>com.apple.compilers.llvm.clang.1_0</string>
	</dict>
	<key>SupportedTargets</key>
	<dict>
		<key>macosx</key>
		<dict>
			<key>Archs</key>
			<array>
				<string>${arch}</string>
			</array>
			<key>DefaultDeploymentTarget</key>
			<string>${deployment}</string>
			<key>PlatformFamilyName</key>
			<string>macOS</string>
		</dict>
	</dict>
</dict>
</plist>
PLIST
