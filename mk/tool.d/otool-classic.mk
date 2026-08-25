# otool-classic -- cctools' otool, with the arm/arm64/i386 disassemblers.
# A stock toolchain ships the cctools build under this name and points
# plain `otool` at llvm-otool, so we leave `otool` for stage 5.
#
# -DOTOOL is what otool.xcconfig adds on top of public_tool.xcconfig; the
# shared print sources use it to select otool's output conventions.
T_CFLAGS+=	-DOTOOL

# The disassemblers call __cxa_demangle to pretty-print C++ symbols
# (SymbolLookUp in arm64_disasm.c, arm_disasm.c, i386_disasm.c), which
# lives in libc++abi.  Apple pulls it in the same way, via -lc++.
T_LDADD+=	-lc++
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
