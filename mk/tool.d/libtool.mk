# libtool -- cctools' static library tool.  ranlib is the same binary,
# dispatched on argv[0]; a stock toolchain ships ranlib as a symlink to
# it, and T_LINKS gives us the same pair.
#
# libtool.c calls make_obj_file_with_linker_options(), which Apple keeps
# in libcctoolshelper -- a library absent from both the cctools release
# and any shipped Xcode.  src/cctools-helpers/ is our BSD-licensed
# reimplementation of it, and include/mach-o/cctools_helpers.h supplies
# the declaration libtool.c includes.  The submodule itself is untouched.
T_SRCS=	libtool.c
T_SRCS+=	src/cctools-helpers/cctools_helpers.c
T_LINKS=	ranlib
.include "${TOP}/mk/with-libstuff.mk"
.include "${TOP}/mk/with-libmacho.mk"
