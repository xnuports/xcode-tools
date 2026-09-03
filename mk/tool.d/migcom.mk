# migcom -- the Mach Interface Generator's compiler.
#
# mig turns a .defs file into the C a caller and a server need to talk
# across a Mach port.  Much of what an SDK carries under mach/ is its
# output rather than a file anyone wrote: mach/clock.h, mach_port.h,
# task.h and a dozen more are generated from xnu's libsyscall/mach/*.defs.
#
# Without it this tree had to leave those headers out, which is why
# anything reaching for mach/clock.h stopped.  mig itself is the shell
# script beside this; migcom is the program it runs, and it looks for
# it at ../libexec/migcom relative to itself, which is where Apple puts
# it and where this installs it.

# handler.c is in mig.xcodeproj's file list but not in its Sources
# phase, so Apple does not compile it -- and it no longer compiles:
# it defines WriteIncludes static against a non-static declaration and
# reads IsCamelot and IsKernel, which nothing declares any more.  A
# glob over the directory picks it up regardless, so the sources are
# named here, and they are the thirteen Apple's Sources phase names.
T_SRCS=		error.c global.c header.c lexxer.l mig.c parser.y \
		routine.c server.c statement.c string.c type.c \
		user.c utils.c

# The lexer and parser are generated into the object directory, so a
# quoted #include in them no longer resolves beside the source it came
# from.  Point the compiler back at it.
T_CFLAGS+=	-I${TOP}/src/apple/distribution-Developer_Tools/bootstrap_cmds/migcom.tproj

# The lexer includes y.tab.h, which is what yacc has always called its
# token header.  This tree names it after the grammar -- parser.tab.h --
# so the classic name is provided beside it rather than changing what
# the rest of the tree generates.
${T_OBJDIR}/y.tab.h: ${T_OBJDIR}/parser.tab.h
	@cp ${.ALLSRC} ${.TARGET}

${T_OBJDIR}/lexxer.l.lex.o: ${T_OBJDIR}/y.tab.h

# MIG_VERSION is what mig -version prints and what it stamps into every
# file it generates.  Apple's build fills it from
# RC_ProjectNameAndSourceVersion, which this tree has no equivalent of,
# so it is taken from the name of the source drop being built.
MIG_SRCVER!=	git -C ${TOP}/src/apple/distribution-Developer_Tools/bootstrap_cmds \
		    describe --tags 2>/dev/null || echo bootstrap_cmds
T_CFLAGS+=	-DMIG_VERSION=\"${MIG_SRCVER}\"
