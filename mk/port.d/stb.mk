# stb.
#
# TextureConverter lists STB as a back end for BC1, BC3, BC4 and BC5, which
# is exactly what stb_dxt.h provides.  It is one public-domain header with
# no build system and no dependencies, which is the whole reason to have it:
# the cheap, always-available path to the four simplest block formats, for
# when the better encoders are not what is wanted.
#
# Dual-licensed MIT / public domain.  No release tags upstream, so the
# submodule is pinned to a commit; stb_dxt.h itself is at v1.12.
#
# Nothing to build.  The header is copied where the rest of usr/local's
# headers are and that is the whole port.
P_BUILDSYS=	make
P_MAKE=		true
P_NOSTAGE=	yes
P_PROGS=

P_POST_BUILD=	mkdir -p dest/include && cp -f stb_dxt.h dest/include/

P_RELEASE_MERGE=	dest/include usr/local/include
