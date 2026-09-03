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

# The architectures this SDK can build for.  <arch> may name several,
# separated by spaces, and each becomes an entry: a build asks the SDK
# what it supports before it will compile for anything, and one that
# claims a single architecture cannot produce a universal binary.
arch_entries=""

canonical=$1
display=$2
version=$3
deployment=$4
arch=$5

# The oldest macOS this SDK will build for.  swiftc reads the deployment
# range out of SDKSettings.json -- not the plist -- and rejects the file
# outright when the range is not there, saying only that the settings
# "could not be parsed", so leaving it out costs more than the value
# itself is worth.  10.13 is what Apple's own macOS SDK declares.
min_deployment=${XT_MIN_DEPLOYMENT_TARGET:-10.13}

for a in ${arch}; do
	arch_entries="${arch_entries}				<string>${a}</string>
"
done

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
	<!-- clang's DarwinSDKInfo parser takes exactly two keys from the
	     top level, Version and MaximumDeploymentTarget, and gives up
	     on the whole file if either is missing.  Every consumer that
	     reads deployment information through it -- swiftc among them
	     -- then falls back to its own defaults in silence, which is
	     how a Swift build ends up targeting macOS 10.13. -->
	<!-- swift-driver decodes this file too, and its decoder is
	     stricter than clang's in one place: for any SDK whose
	     CanonicalName begins "macosx" it requires VersionMap, and
	     inside it macOS_iOSMac, with no fallback.  Without them the
	     decode throws, the driver reports only that it "could not
	     read SDKSettings.json", and every Swift compilation carries
	     the warning.
	     Empty will not do either, because clang parses the same file
	     and rejects a mapping with no entries -- so between the two
	     the key must be present and populated.  The table below is
	     the macOS-to-Mac-Catalyst correspondence as released.  It is
	     not regular: 11.0 pairs with 14.2 rather than 14.0, and 13.0
	     with 16.1, so it is written out rather than computed.  From
	     macOS 26 the two version numbers agree.
	     Nothing here targets Catalyst, and the driver reads the
	     mapping only when something does. -->
	<key>VersionMap</key>
	<dict>
		<key>macOS_iOSMac</key>
		<dict>
			<key>10.15</key>
			<string>13.1</string>
			<key>10.15.1</key>
			<string>13.2</string>
			<key>10.15.2</key>
			<string>13.3</string>
			<key>10.15.3</key>
			<string>13.3.1</string>
			<key>10.15.4</key>
			<string>13.4</string>
			<key>10.15.5</key>
			<string>13.5</string>
			<key>11.0</key>
			<string>14.2</string>
			<key>11.0.1</key>
			<string>14.2</string>
			<key>11.1</key>
			<string>14.3</string>
			<key>11.2</key>
			<string>14.4</string>
			<key>11.3</key>
			<string>14.5</string>
			<key>11.4</key>
			<string>14.6</string>
			<key>11.5</key>
			<string>14.7</string>
			<key>12.0</key>
			<string>15.0</string>
			<key>12.0.1</key>
			<string>15.0</string>
			<key>12.1</key>
			<string>15.2</string>
			<key>12.2</key>
			<string>15.3</string>
			<key>12.3</key>
			<string>15.4</string>
			<key>12.4</key>
			<string>15.5</string>
			<key>12.5</key>
			<string>15.6</string>
			<key>13.0</key>
			<string>16.1</string>
			<key>13.1</key>
			<string>16.2</string>
			<key>13.2</key>
			<string>16.3</string>
			<key>13.3</key>
			<string>16.4</string>
			<key>13.4</key>
			<string>16.5</string>
			<key>13.5</key>
			<string>16.6</string>
			<key>14.0</key>
			<string>17.0</string>
			<key>14.1</key>
			<string>17.1</string>
			<key>14.2</key>
			<string>17.2</string>
			<key>14.3</key>
			<string>17.3</string>
			<key>14.4</key>
			<string>17.4</string>
			<key>14.5</key>
			<string>17.5</string>
			<key>14.6</key>
			<string>17.6</string>
			<key>15.0</key>
			<string>18.0</string>
			<key>15.1</key>
			<string>18.1</string>
			<key>15.2</key>
			<string>18.2</string>
			<key>15.3</key>
			<string>18.3</string>
			<key>15.4</key>
			<string>18.4</string>
			<key>15.5</key>
			<string>18.5</string>
			<key>15.6</key>
			<string>18.6</string>
			<key>26.0</key>
			<string>26.0</string>
			<key>26.1</key>
			<string>26.1</string>
			<key>26.2</key>
			<string>26.2</string>
			<key>26.3</key>
			<string>26.3</string>
			<key>26.4</key>
			<string>26.4</string>
			<key>26.5</key>
			<string>26.5</string>
		</dict>
	</dict>
	<key>MaximumDeploymentTarget</key>
	<string>${version}.99</string>
	<key>SupportedTargets</key>
	<dict>
		<key>macosx</key>
		<dict>
			<key>Archs</key>
			<array>
${arch_entries}			</array>
			<key>DefaultDeploymentTarget</key>
			<string>${deployment}</string>
			<key>MinimumDeploymentTarget</key>
			<string>${min_deployment}</string>
			<key>MaximumDeploymentTarget</key>
			<string>${version}.99</string>
			<key>LLVMTargetTripleSys</key>
			<string>macos</string>
			<key>LLVMTargetTripleVendor</key>
			<string>apple</string>
			<key>PlatformFamilyName</key>
			<string>macOS</string>
		</dict>
	</dict>
</dict>
</plist>
PLIST
