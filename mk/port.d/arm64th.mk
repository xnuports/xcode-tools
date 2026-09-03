# arm64th -- a Forth interpreter for Apple Silicon, written in arm64
# assembly.
#
# Nothing to do with building code, so usr/local/bin.  Its Makefile
# names the binary forth, not arm64th, and that is what installs.
P_BUILDSYS=	make
P_NOSTAGE=	yes
P_PROGS=	forth
