# TextureAtlas -- packs a folder of images into a SpriteKit .atlasc bundle.
# ImageIO reads and writes the pages, CoreGraphics decodes into a bitmap and
# libz gzips the PVR pages; Apple links the same set.
T_SRCS=		TextureAtlas.m maxrects.c introsort.c
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreGraphics
T_LDADD+=	-framework ImageIO
T_LDADD+=	-lz
