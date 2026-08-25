# size-classic -- cctools' size.  A stock toolchain ships this name as
# the real binary and makes `size` a symlink to it; T_LINKS gives us the
# same pair (a hard link rather than a symlink -- size does not dispatch
# on argv[0], so the two are interchangeable here).
T_SRCS=	size.c
T_LINKS=	size
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
