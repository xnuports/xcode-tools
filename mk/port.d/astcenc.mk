# ARM's astc-encoder.
#
# ASTC is the compressed texture format Metal actually wants on Apple
# hardware, and TextureConverter offers every block size of it in both LDR
# and HDR.  Apple's links ARM's encoder for that; so does ours.  Writing an
# ASTC encoder from scratch would be writing a worse one -- the format's
# whole difficulty is the search, and this is the reference implementation
# of that search.
#
# Apache-2.0, which this tree can carry.
#
# Only the library is wanted.  ASTCENC_CLI would build astcenc(1), which is
# a texture tool of its own and not the one being replaced here.
P_BUILDSYS=	cmake
P_CMAKE_SRC=	.
P_OBJDIR=	${P_WORKDIR}/build
P_NOSTAGE=	yes
P_PROGS=

# NEON, singly, rather than the universal build cmake turns on by default
# for Apple: this tree builds for the machine it runs on, and a universal
# library would carry an x86 half nothing here links.  WERROR off for the
# reason every imported component has it off (mk/xcodetools.sys.mk).
P_CONFIGURE_ARGS=	-DASTCENC_CLI=OFF \
			-DASTCENC_UNIVERSAL_BUILD=OFF \
			-DASTCENC_ISA_NEON=ON \
			-DASTCENC_WERROR=OFF

# cmake's install rules cover the command line tool and the shared library,
# neither of which is built here, so the static library and its one public
# header are gathered by hand -- as zstd's are, and for the same reason.
P_POST_BUILD=	mkdir -p dest/lib dest/include && \
		cp -f Source/libastcenc-neon-static.a dest/lib/libastcenc.a && \
		cp -f ${TOP}/src/extras/astc-encoder/Source/astcenc.h \
		    dest/include/

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
