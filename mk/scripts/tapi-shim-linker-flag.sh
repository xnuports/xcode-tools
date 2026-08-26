#!/bin/sh
#
# tapi-shim-linker-flag.sh -- let tapi configure against a modern LLVM.
#
# tapi's CMakeLists calls llvm_check_linker_flag(), which came from
# llvm/cmake/modules/LLVMCheckLinkerFlag.cmake.  That module is gone from
# current llvm-project -- CMake's own check_linker_flag replaced it -- so
# configuring tapi as an LLVM external project fails with:
#
#	Unknown CMake command "llvm_check_linker_flag".
#
# Insert a definition of the old name in terms of the new one, directly
# above the first call, so that project() has already run by then.
#
# Run from the root of a *copy* of the tapi tree; the submodule is
# read-only.  Idempotent.
#
# Copyright (c) 2026 Sunneva N. Mariu
# SPDX-License-Identifier: BSD-3-Clause

set -e

shim="${1:?usage: $0 <path to shim .cmake>}"
cml="CMakeLists.txt"

[ -f "$cml" ]  || { echo "tapi-shim: no $cml in $(pwd)" >&2; exit 1; }
[ -f "$shim" ] || { echo "tapi-shim: no shim at $shim" >&2; exit 1; }

if grep -q 'xcode_tools_llvm_check_linker_flag_shim' "$cml"; then
	exit 0
fi

n=$(grep -n 'llvm_check_linker_flag' "$cml" | head -1 | cut -d: -f1)
[ -n "$n" ] || exit 0

{
	head -n $((n - 1)) "$cml"
	cat "$shim"
	tail -n +"$n" "$cml"
} > "$cml.new"
mv "$cml.new" "$cml"

echo "  shimmed llvm_check_linker_flag in tapi/CMakeLists.txt"
