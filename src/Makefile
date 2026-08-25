# src/Makefile
#
# Drives the build of every program listed in mk/progs.mk by recursing
# into mk/tool.mk once per entry.  The three fields of each entry map
# round-robin onto d (source dir), p (program name) and b (release
# suffix).

TOP?=		${.CURDIR}/..

.include "${TOP}/mk/xcodetools.sys.mk"
.include "${TOP}/mk/progs.mk"

all:
.for d p b in ${PROGS}
.if empty(DISABLED_PROGS:M${p})
	-@${MAKE} -f ${TOP}/mk/tool.mk TOP=${TOP} \
		T_DIR=${d} T_PROG=${p} T_BIN=${b}
.endif
.endfor

clean:
.for d p b in ${PROGS}
.if empty(DISABLED_PROGS:M${p})
	-@${MAKE} -f ${TOP}/mk/tool.mk TOP=${TOP} \
		T_DIR=${d} T_PROG=${p} T_BIN=${b} clean
.endif
.endfor

# Every recursion above is prefixed with '-', so one tool failing to
# build does not stop the rest -- but that also means a broken tool
# silently vanishes from the release tree.  check-progs is the guard:
# it fails if any inventory entry did not produce its binary.
check-progs:
.for d p b in ${PROGS}
.if empty(DISABLED_PROGS:M${p})
	@test -e ${TOP}/build/release/${b}/${p} || \
		{ ${ECHO} "MISSING: ${b}/${p}  (from src/${d})"; exit 1; }
.endif
.endfor
	@${ECHO} "check: every inventory entry produced a binary"

list-progs:
.for d p b in ${PROGS}
.if empty(DISABLED_PROGS:M${p})
	@${ECHO} "${b}/${p}	<- src/${d}"
.endif
.endfor

.PHONY: all clean check-progs list-progs
