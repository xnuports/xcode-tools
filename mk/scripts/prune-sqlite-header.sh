#!/bin/sh
#
# prune-sqlite-header.sh -- drop declarations the system library does not have.
#
# Apple do not publish their sqlite, and their sqlite3.h is not upstream's:
# it renames the include guard, carries a hundred and twenty availability
# annotations, and declares nineteen fewer functions than upstream 3.51.0 --
# nothing upstream does not have, but a set their libsqlite3 does not export.
#
# Most of that set is already behind a feature macro and never reaches a
# translation unit.  These are the ones that do, and every one of them would
# compile against this SDK and then fail to link against the system's
# libsqlite3.  Removing them is the difference between a header that
# describes the library and one that describes a library nobody has.
#
# The annotations and the guard name are not reproduced.  They exist only in
# Apple's own header, and this tree builds from source rather than copying
# what Apple ship.
#
# A symbol that is not found is an error rather than a shrug: it means the
# upstream header changed shape and this list needs looking at, which is
# exactly the moment a silent no-op would hide.
#
# Usage: prune-sqlite-header.sh <sqlite3.h> <symbol>...
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

[ $# -ge 2 ] || { echo "usage: $0 <sqlite3.h> <symbol>..." >&2; exit 1; }

HEADER="$1"
shift

[ -f "${HEADER}" ] || { echo "$0: no header at ${HEADER}" >&2; exit 1; }

for symbol in "$@"; do
	python3 - "${HEADER}" "${symbol}" <<'PYEOF'
import re, sys

path, symbol = sys.argv[1], sys.argv[2]
lines = open(path).readlines()

# A declaration begins at a line starting with SQLITE_API that names the
# symbol, and runs to the line ending the parameter list.  Several sit on one
# line; sqlite3_unlock_notify takes five.
start = None
call = re.compile(r'\b%s\s*\(' % re.escape(symbol))
for i, line in enumerate(lines):
    if line.startswith('SQLITE_API') and call.search(line):
        start = i
        break

if start is None:
    sys.exit("%s: %s is not declared in %s" % (sys.argv[0], symbol, path))

end = start
while not lines[end].rstrip().endswith(');'):
    end += 1
    if end >= len(lines):
        sys.exit("%s: declaration of %s is unterminated" % (sys.argv[0], symbol))

del lines[start:end + 1]
open(path, 'w').writelines(lines)
PYEOF
done

echo "    sqlite3.h: $# declarations pruned"
