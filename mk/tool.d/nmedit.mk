# nmedit -- built from strip.c with -DNMEDIT, which selects the
# symbol-editing entry points instead of the stripping ones.  cctools
# has no nmedit.c; the shipped tool is this same source twice-compiled.
T_SRCS=	strip.c
T_CFLAGS+=	-DNMEDIT
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
