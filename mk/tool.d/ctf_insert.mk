# ctf_insert -- one of the single-file programs in cctools' flat misc/ directory,
# so the source list has to be pinned or auto-discovery would compile the
# whole directory into one binary.
T_SRCS=	ctf_insert.c
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
