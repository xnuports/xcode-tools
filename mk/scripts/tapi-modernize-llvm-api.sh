#!/bin/sh
#
# tapi-modernize-llvm-api.sh -- adapt tapi to renamed LLVM APIs.
#
# tapi 2.0.0 was written against an older LLVM.  Since then StringRef's
# startswith/endswith were renamed to the STL-style starts_with and
# ends_with, and the old spellings were removed, so tapi no longer
# compiles against current llvm-project:
#
#	error: no member named 'startswith' in 'llvm::StringRef'
#
# StringRef::equals went the same way, replaced by operator==.  Note the
# negated form has to be rewritten first: turning "!a.equals(b)" into
# "!a == (b)" parses as "(!a) == b", which is both wrong and a compile
# error on StringRef.  It becomes "a != (b)" instead.
#
# clang's FileManager::getDirectory and ::getFile are gone;
# getOptionalDirectoryRef and getOptionalFileRef are the modern
# spellings and, returning std::optional<...EntryRef>, behave the same in
# the boolean and value contexts tapi uses them in.
#
# llvm/CodeGen/LowLevelType.h moved to llvm/CodeGenTypes/.
#
# lib/Core/FakeSymbols.cpp is a stub file: it defines a handful of LLVM
# object-file entry points as llvm_unreachable so tapi does not have to
# link the real implementations.  Being definitions, they have to track
# LLVM's declarations exactly, and TapiUniversal::create differs across
# LLVM branches -- some declare a SkipUnknownTriples parameter, some do
# not, and tapi drops disagree with them in both directions.  So rather
# than assume, read what this LLVM declares and match it.
#
# Every edit here is mechanical and semantics-preserving.  The method
# rewrites require a leading dot, so unrelated identifiers are untouched.
#
# Run from the root of a *copy* of the tapi tree; the submodule is
# read-only.  Idempotent.
#
# Copyright (c) 2026 Sunneva N. Mariu
# SPDX-License-Identifier: BSD-3-Clause

set -e

changed=0

for f in $(grep -rl -E '\.(startswith|endswith|equals)\(|get(Directory|File)\(|llvm/CodeGen/LowLevelType\.h' \
		--include='*.cpp' --include='*.h' . 2>/dev/null); do
	sed -i.bak \
		-e 's|\.startswith(|.starts_with(|g' \
		-e 's|\.endswith(|.ends_with(|g' \
		-e 's|!\([A-Za-z_][A-Za-z0-9_]*\)\.equals(|\1 != (|g' \
		-e 's|\.equals(| == (|g' \
		-e 's|getDirectory(|getOptionalDirectoryRef(|g' \
		-e 's|\.getFile(|.getOptionalFileRef(|g' \
		-e 's|->getFile(|->getOptionalFileRef(|g' \
		-e 's|llvm/CodeGen/LowLevelType\.h|llvm/CodeGenTypes/LowLevelType.h|g' \
		"$f"
	rm -f "$f.bak"
	changed=$((changed + 1))
done

[ "$changed" -gt 0 ] && \
	echo "  modernized StringRef calls in $changed tapi files"

# FakeSymbols.cpp stubs must match LLVM's current declarations.  Which
# way to adjust depends on the LLVM being built against, so consult its
# header rather than assuming.
fs="lib/Core/FakeSymbols.cpp"
hdr="${1:-}/llvm/include/llvm/Object/TapiUniversal.h"

if [ -f "$fs" ] && [ -f "$hdr" ]; then
	if grep -q 'create(MemoryBufferRef Source, *bool' "$hdr"; then
		want_param=1
	else
		want_param=0
	fi

	if grep -q 'TapiUniversal::create(MemoryBufferRef Source, *bool' "$fs"; then
		has_param=1
	else
		has_param=0
	fi

	if [ "$want_param" = 1 ] && [ "$has_param" = 0 ]; then
		sed -i.bak \
			-e 's|TapiUniversal::create(MemoryBufferRef Source)|TapiUniversal::create(MemoryBufferRef Source, bool SkipUnknownTriples)|' \
			"$fs"
		rm -f "$fs.bak"
		echo "  added SkipUnknownTriples to the TapiUniversal::create stub"
	elif [ "$want_param" = 0 ] && [ "$has_param" = 1 ]; then
		sed -i.bak \
			-e 's|TapiUniversal::create(MemoryBufferRef Source, *bool [A-Za-z_]*)|TapiUniversal::create(MemoryBufferRef Source)|' \
			"$fs"
		rm -f "$fs.bak"
		echo "  removed SkipUnknownTriples from the TapiUniversal::create stub"
	fi
fi

exit 0
