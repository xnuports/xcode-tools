# zstd.
#
# Not something Xcode ships, and not wanted for its own sake: a .xcresult
# bundle stores its objects zstd-compressed, and nothing on this system
# provides the library.  Apple's xcresulttool links no zstd of its own --
# being Swift and sixteen megabytes, it carries one inside -- and there is
# no libzstd.dylib in the shared cache and no zstd.h in their SDK, so this
# builds one.
#
# Only the library is built.  zstd's own Makefile builds a command-line
# tool, a legacy-format reader and a pile of tests besides, none of which
# anything here wants.
#
# BSD-3-Clause, like this tree's own tools.
P_BUILDSYS=	make

# As in the sqlite port: a command-line variable rides into child makes
# through MAKEFLAGS, and TOP means something else inside someone else's
# build.
P_MAKE=		env -u MAKEFLAGS make
P_MAKE_ARGS=	-C lib libzstd.a
P_NOSTAGE=	yes

P_PROGS=

# Only the library and its header, gathered first: zstd's lib/ is its source
# tree, and merging that whole directory put forty-odd source files and
# subdirectories into usr/local/lib.
P_POST_BUILD=	mkdir -p dest/lib dest/include && \
		cp -f lib/libzstd.a dest/lib/ && \
		cp -f lib/zstd.h lib/zstd_errors.h dest/include/

P_RELEASE_MERGE=	dest/lib usr/local/lib \
			dest/include usr/local/include
