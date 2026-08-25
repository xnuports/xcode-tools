# install_name_tool -- one of the single-file programs in cctools' flat misc/ directory,
# so the source list has to be pinned or auto-discovery would compile the
# whole directory into one binary.
T_SRCS=	install_name_tool.c
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
