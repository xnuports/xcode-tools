# TextureConverter -- the tool around the compressor ports.
#
# ImageIO reads the input images and CoreGraphics decodes them, which is
# what Apple's links for the same job.  The compressor libraries in
# usr/local come from src/extras (see mk/port.d/{astcenc,stb,etc2comp,nvtt});
# nothing links them yet, because the modes that would are still to write.
T_SRCS=		TextureConverter.m ktx.c formats.c mipmap.cpp
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreGraphics
T_LDADD+=	-framework ImageIO

# NVTT, for its filters.  Apple's mip chains are its polyphase Kaiser --
# see mk/port.d/nvtt.mk -- so the chains here come out bit for bit theirs.
T_CFLAGS+=	-I${TOP}/build/release/usr/local/include
T_CXXFLAGS+=	-std=c++11 -DNV_CPU_AARCH64=1 -DNV_CPU_ARM=1
T_LDADD+=	${TOP}/build/release/usr/local/lib/libnvtt-bc.a
