# pip.
#
# Xcode ships pip beside its python3 -- two console scripts in
# Developer/usr/bin and the package in that python's site-packages -- so
# this installs it the same way.  mk/scripts/install-pip.sh does the work
# and says there why pip cannot install itself here.
#
# This has to follow python/cpython in PORTS: the script runs the python3
# that port just installed, both to read pyproject.toml and to be sure the
# interpreter pip is being laid down for actually works.
#
# The version is cpython's, not a constant: it names the site-packages
# directory and the second script, and both have to match whatever the
# submodule is at.
# All on one line, because a "!=" command is not continued across
# backslash-newlines: splitting it leaves bmake running "awk '/^" on its own.
# The "#" of each #define is escaped for the same reason it would be anywhere
# else in a makefile -- unescaped, it starts a comment and takes the rest of
# the command with it.
PY_VERSION!=	awk '/^\#define PY_MAJOR_VERSION/{maj=$$3} /^\#define PY_MINOR_VERSION/{min=$$3} END{print maj "." min}' ${TOP}/src/python/cpython/Include/patchlevel.h

P_COPY=			no
P_BUILDSYS=		make
P_MAKE=			sh ${TOP}/mk/scripts/install-pip.sh
P_MAKE_ARGS=		${TOP}/src/python/pip \
			${TOP}/build/release/usr/bin/python3 \
			${PY_VERSION} \
			${P_OBJDIR}
P_NOSTAGE=		yes

P_PROGS=		bin/pip3 bin/pip${PY_VERSION}
P_RELEASE_MERGE=	lib usr/lib
