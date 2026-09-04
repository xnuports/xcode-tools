#!/bin/sh
#
# Install pip the way Xcode ships it.
#
# Xcode puts two console scripts in Developer/usr/bin -- pip3 and pip3.<minor>,
# both the same seven lines, both running through xcrun so they find whichever
# python3 the selected developer directory provides -- and the package itself
# in that python's site-packages beside a dist-info directory.  This builds
# the same shape.
#
# pip normally installs itself, and cannot here: its build backend is
# flit-core, which is not vendored, and fetching it would mean reaching out
# to the network in the middle of a build.  So the wheel pip would have built
# is assembled directly instead -- the package tree copied, and the metadata
# generated from pyproject.toml rather than transcribed, so it follows the
# checkout rather than drifting from it.
#
# Usage: install-pip.sh <pip-checkout> <python3> <x.y> <destdir>
#
set -e

PIP_SRC=$1
PYTHON=$2
PYVER=$3
DEST=$4

if [ $# -ne 4 ]; then
	echo "usage: $0 <pip-checkout> <python3> <x.y> <destdir>" >&2
	exit 1
fi

# The version comes out of cpython's patchlevel.h and decides both the
# site-packages path and the name of the second console script.  An empty or
# malformed one would install pip somewhere no interpreter looks, quietly, so
# it is checked rather than trusted.
case "${PYVER}" in
[0-9]*.[0-9]*) ;;
*)
	echo "$0: python version '${PYVER}' is not x.y" >&2
	exit 1
	;;
esac

SITE=${DEST}/lib/python${PYVER}/site-packages
VERSION=$("${PYTHON}" - "${PIP_SRC}" <<'PYEOF'
import re, sys, pathlib
text = (pathlib.Path(sys.argv[1]) / "src" / "pip" / "__init__.py").read_text()
print(re.search(r'__version__\s*=\s*"([^"]+)"', text).group(1))
PYEOF
)
DISTINFO=${SITE}/pip-${VERSION}.dist-info

rm -rf "${DEST}/bin" "${DEST}/lib"
mkdir -p "${SITE}" "${DEST}/bin" "${DISTINFO}"

# The package.  __pycache__ is left behind: it is the host's, keyed to a
# magic number that need not be the target's.
rsync -a --exclude '__pycache__' "${PIP_SRC}/src/pip/" "${SITE}/pip/"

# The console scripts.  Both are the same file under two names, which is
# what Xcode ships; the shebang is xcrun rather than a path so the script
# follows xcode-select the way every other tool here does.
for name in pip3 "pip${PYVER}"; do
	cat > "${DEST}/bin/${name}" <<'SCRIPT'
#!/usr/bin/xcrun python3
# -*- coding: utf-8 -*-
import re
import sys
from pip._internal.cli.main import main
if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
SCRIPT
	chmod 755 "${DEST}/bin/${name}"
done

# The metadata, read out of pyproject.toml so it says what the checkout says.
"${PYTHON}" - "${PIP_SRC}" "${DISTINFO}" "${VERSION}" "${SITE}" "${DEST}" <<'PYEOF'
import base64, hashlib, pathlib, sys, tomllib

src, distinfo, version, site, dest = (pathlib.Path(p) for p in sys.argv[1:6])
version = sys.argv[3]

project = tomllib.loads((src / "pyproject.toml").read_text())["project"]

# license-files is a list of globs, and pip's expands to every vendored
# library's licence as well as its own.  Metadata 2.4 names them in the
# headers and keeps the files under licenses/.
licences = []
for pattern in project.get("license-files", []):
    licences += [p.relative_to(src) for p in sorted(src.glob(pattern)) if p.is_file()]

lines = [
    "Metadata-Version: 2.4",
    f"Name: {project['name']}",
    f"Version: {version}",
    f"Summary: {project['description']}",
]
for author in project.get("authors", []):
    lines.append(f"Author-email: {author['name']} <{author['email']}>")
if "license" in project:
    lines.append(f"License-Expression: {project['license']}")
lines += [f"License-File: {name}" for name in licences]
for name, url in project.get("urls", {}).items():
    lines.append(f"Project-URL: {name}, {url}")
lines += [f"Classifier: {c}" for c in project.get("classifiers", [])]
if "requires-python" in project:
    lines.append(f"Requires-Python: {project['requires-python']}")

readme = src / project["readme"]
lines += ["Description-Content-Type: text/x-rst", "", readme.read_text()]
(distinfo / "METADATA").write_text("\n".join(lines))

for name in licences:
    target = distinfo / "licenses" / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((src / name).read_bytes())

(distinfo / "WHEEL").write_text(
    "Wheel-Version: 1.0\n"
    "Generator: xcode-tools install-pip.sh\n"
    "Root-Is-Purelib: true\n"
    "Tag: py3-none-any\n"
)
(distinfo / "INSTALLER").write_text("xcode-tools\n")

entry_points = ["[console_scripts]"]
for name, target in project.get("scripts", {}).items():
    entry_points.append(f"{name} = {target}")
(distinfo / "entry_points.txt").write_text("\n".join(entry_points) + "\n")

# RECORD, last, so it can hash everything else.  Paths are relative to
# site-packages, which puts the console scripts above it.
def record_line(path):
    data = path.read_bytes()
    digest = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode()
    try:
        name = path.relative_to(site)
    except ValueError:
        name = pathlib.Path("..") / ".." / ".." / path.relative_to(dest)
    return f"{name},sha256={digest},{len(data)}"

paths = sorted(p for p in site.rglob("*") if p.is_file())
paths += sorted(p for p in (dest / "bin").iterdir() if p.is_file())
(distinfo / "RECORD").write_text(
    "\n".join(record_line(p) for p in paths) + f"\n{distinfo.relative_to(site)}/RECORD,,\n"
)
PYEOF

echo "pip ${VERSION} installed for python ${PYVER}"
