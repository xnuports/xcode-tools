# jonesforth -- Richard W.M. Jones's literate FORTH, ported to macOS on
# arm64.  One assembly source and nothing linked; usr/local/bin, beside
# arm64th's "forth", because it is a general utility and not a Mach-O or
# build tool.
#
# The kernel in jonesforth.S has only the assembly primitives: IF, DO,
# ." and the rest are written in FORTH itself, in jonesforth.f, which
# has to be read before the terminal.  So the file is installed as well,
# and jonesforth-repl is the two-line wrapper upstream ships that pipes
# it in ahead of stdin.
T_SRCS=		jonesforth.S

JF_SHARE=	${TOP}/build/release/usr/local/share/jonesforth
JF_REPL=	${TOP}/build/release/usr/local/bin/jonesforth-repl

all: jonesforth-runtime

jonesforth-runtime: .PHONY
	@mkdir -p ${JF_SHARE} ${JF_REPL:H}
	@cp ${T_SRCDIR}/jonesforth.f ${JF_SHARE}/jonesforth.f
	@printf '%s\n' '#!/bin/sh' \
	    'exec cat /usr/local/share/jonesforth/jonesforth.f - |' \
	    '    exec /usr/local/bin/jonesforth "$$@"' > ${JF_REPL}
	@chmod 755 ${JF_REPL}
	@${ECHO} "built: usr/local/bin/jonesforth-repl (script)"
