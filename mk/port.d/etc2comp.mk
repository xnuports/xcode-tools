# Google's etc2comp.
#
# TextureConverter names ETC2COMP as the back end for EAC_R11, EAC_RG11,
# EAC_RGBA8, ETC2_RGB8 and ETC2_RGB8A1, which is exactly this library's
# format list.
#
# Apache-2.0.  Upstream carries no release tags, so the submodule is pinned
# to a commit.
#
# Built by compiling EtcLib directly rather than through its own build
# system.  etc2comp's CMakeLists asks for cmake 2.8.9, which cmake has
# refused since 4.0, and its EtcLib subdirectory cannot be configured on its
# own because it declares no minimum at all.  Patching either is not on --
# submodule contents are left alone -- so the fifteen sources are compiled
# here.  The wildcards are deliberate: a file added upstream is picked up
# rather than quietly dropped.
P_BUILDSYS=	make
P_MAKE=		sh -c
P_MAKE_ARGS=	'set -e; mkdir -p dest/lib dest/include; \
		 c++ -std=c++11 -O2 -c -IEtcLib/Etc -IEtcLib/EtcCodec \
		     EtcLib/Etc/*.cpp EtcLib/EtcCodec/*.cpp; \
		 ar rcs dest/lib/libEtcLib.a *.o; \
		 cp -f EtcLib/Etc/*.h EtcLib/EtcCodec/*.h dest/include/'
P_NOSTAGE=	yes
P_PROGS=

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
