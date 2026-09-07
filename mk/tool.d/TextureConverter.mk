# TextureConverter -- the tool around the compressor ports.
#
# ImageIO reads the input images and CoreGraphics decodes them, which is
# what Apple's links for the same job.  The compressor libraries in
# usr/local come from src/extras (see mk/port.d/{astcenc,stb,etc2comp,nvtt}).
T_SRCS=		TextureConverter.m ktx.c formats.c compress.c mipmap.cpp \
		nvtt.cpp stb.c etc2.cpp decode.cpp eac.c ktx2.c header.c dds.c \
		gamma.cpp
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreGraphics
T_LDADD+=	-framework ImageIO

# NVTT, for the BC formats and for its filters.  Apple's tool answers
# "Using Compressor: NVTT" for every one of BC1 through BC7, and its mip
# chains are NVTT's polyphase Kaiser -- see mk/port.d/nvtt.mk -- so both
# come out bit for bit theirs rather than a second implementation.
T_CFLAGS+=	-I${TOP}/build/release/usr/local/include
T_CXXFLAGS+=	-std=c++11 -DNV_CPU_AARCH64=1 -DNV_CPU_ARM=1
T_LDADD+=	${TOP}/build/release/usr/local/lib/libnvtt-bc.a

# ARM's astc-encoder, which is what Apple's ASTC back end is.
T_LDADD+=	${TOP}/build/release/usr/local/lib/libastcenc.a

# Google's etc2comp, which is Apple's back end for every ETC2 and EAC
# format at every quality.
T_LDADD+=	${TOP}/build/release/usr/local/lib/libEtcLib.a
