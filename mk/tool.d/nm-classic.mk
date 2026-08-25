# nm-classic -- cctools' nm.  A stock Xcode toolchain ships the cctools
# build under this name and points plain `nm` at llvm-nm, so we do the
# same and leave `nm` for llvm-project (stage 5).
T_SRCS=	nm.c
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
