# xcode-select -- developer directory selection.
T_SRCS=	xcode-select.c
.include "${TOP}/mk/with-devpath.mk"

# Apple's xcode-select links libxcselect; so does ours, and for the
# same reason -- it is where the developer directory is decided.
.include "${TOP}/mk/with-xcselect.mk"
