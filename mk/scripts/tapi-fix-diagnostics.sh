#!/bin/sh
#
# tapi-fix-diagnostics.sh -- satisfy clang tblgen's diagnostic style rules.
#
# clang-tblgen enforces two rules on diagnostic text that tapi 2.0.0
# predates, and it rejects the whole .td file over them:
#
#	Diagnostics should not start with a capital letter
#	Diagnostics should not end with punctuation
#
# The capital-letter rule has a hardcoded exemption list in clang's
# ClangDiagnosticsEmitter.cpp (Clang, OpenMP, Neon, ...) with no way to
# extend it or turn the check off, so the text itself has to change.
#
# The edits are deliberately minimal and preserve meaning: the three
# messages that open with an identifier get a lowercase article in front,
# which keeps the identifier spelled exactly as it was, and the five that
# end in a full stop simply lose it.
#
# Run from the root of a *copy* of the tapi tree; the submodule is
# read-only.  Idempotent.
#
# Copyright (c) 2026 Sunneva N. Mariu
# SPDX-License-Identifier: BSD-3-Clause

set -e

td="include/tapi/Diagnostics/DiagnosticTAPIKinds.td"
[ -f "$td" ] || { echo "tapi-fix-diagnostics: no $td in $(pwd)" >&2; exit 1; }

before=$(cksum < "$td")

# Rule 1: must not start with a capital.  Prefix an article rather than
# lowercasing, so the flag names stay correct.
sed -i.bak \
	-e 's|Error<"ApplicationExtensionSafe flag|Error<"the ApplicationExtensionSafe flag|' \
	-e 's|Error<"NotForDyldSharedCache flag|Error<"the NotForDyldSharedCache flag|' \
	-e 's|Warning<"Auto zippering|Warning<"auto zippering|' \
	"$td"

# Rule 2: must not end with punctuation.  Drop the trailing full stop
# just before the closing quote.
sed -i.bak2 -e 's|\.">;$|">;|' "$td"

rm -f "$td.bak" "$td.bak2"

if [ "$(cksum < "$td")" != "$before" ]; then
	echo "  adjusted tapi diagnostics for clang-tblgen style rules"
fi
