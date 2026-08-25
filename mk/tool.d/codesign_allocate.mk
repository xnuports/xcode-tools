# codesign_allocate -- one of the single-file programs in cctools' flat misc/ directory,
# so the source list has to be pinned or auto-discovery would compile the
# whole directory into one binary.
T_SRCS=	codesign_allocate.c
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"

# codesign_allocate.xcconfig embeds an Info.plist in __TEXT.
T_LDADD+=	-sectcreate __TEXT __info_plist \
		${CCTOOLS}/misc/codesign_allocate-Info.plist
