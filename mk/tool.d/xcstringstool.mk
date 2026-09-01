# xcstringstool -- string catalogs.
# T_SRCS is explicit because json.c lives outside this directory.
T_SRCS=	xcstringstool.c xs_compile.c
.include "${TOP}/mk/with-json.mk"

# Property lists are written with CoreFoundation, as Apple's own
# xcstringstool does -- see the note in xs_compile.c.
T_LDADD+=	-framework CoreFoundation
