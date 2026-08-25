# flex -- only the flex binary is wanted, not libfl.
#
# Its bundled libtool links libfl as a dylib with -flat_namespace, which
# modern ld refuses for shared-cache-eligible dylibs ("Shared cache
# eligible dylibs cannot use '-flat_namespace'").  Nothing here needs
# the library, so the shared build is turned off entirely.
P_CONFIGURE_ARGS+=	--disable-shared
P_PROGS=	bin/flex
