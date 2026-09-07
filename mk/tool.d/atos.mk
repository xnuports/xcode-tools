# atos -- addresses to symbols.
#
# Apple's is a thin front end over CoreSymbolicationDT, a private framework.
# What is behind it is DWARF in a Mach-O or a dSYM, and llvm-project -- which
# this tree already builds -- reads that.  So the link is against LLVM's own
# symbolizer rather than a new dependency: libdwarf is LGPL-2.1, which a
# BSD-3-Clause tree should not statically carry, and atosl has been archived
# since 2015 and knows no x86_64.
#
# The archives come out of the llvm port's build directory rather than the
# release tree.  They are build products of a component this tree compiles,
# not something a Developer directory ships, and staging a hundred megabytes
# of static archives to install one program would be the wrong trade.
LLVM_BUILD=	${TOP}/build/ports/llvm/build

# Each "!=" is on one line: the command is not continued across
# backslash-newlines.
#
# The archives are named in full rather than found through the -L that
# llvm-config hands out.  That directory holds LLVM's own libc++, so putting
# it on the search path links the program against an @rpath copy that is not
# there at run time; naming the archives leaves libc++ to the system.
LLVM_CXXFLAGS!=	${TOP}/build/ports/llvm/build/bin/llvm-config --cxxflags 2>/dev/null || echo ""
LLVM_ARCHIVES!=	${TOP}/build/ports/llvm/build/bin/llvm-config --libs symbolize debuginfodwarf object 2>/dev/null | tr ' ' '\n' | sed -e "s|^-l|${TOP}/build/ports/llvm/build/lib/lib|" -e "s|$$|.a|" | tr '\n' ' ' || echo ""
LLVM_SYSLIBS!=	${TOP}/build/ports/llvm/build/bin/llvm-config --system-libs 2>/dev/null || echo ""

T_SRCS=		atos.cpp
T_CXXFLAGS+=	${LLVM_CXXFLAGS:N-W*:N-pedantic:N-fno-exceptions}
T_LDADD+=	${LLVM_ARCHIVES} ${LLVM_SYSLIBS}
