# TextureConverter -- the tool around the compressor ports.
#
# ImageIO reads the input images and CoreGraphics decodes them, which is
# what Apple's links for the same job.  The compressor libraries in
# usr/local come from src/extras (see mk/port.d/{astcenc,stb,etc2comp,nvtt});
# nothing links them yet, because the modes that would are still to write.
T_SRCS=		TextureConverter.m ktx.c formats.c
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreGraphics
T_LDADD+=	-framework ImageIO
