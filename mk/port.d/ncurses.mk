# ncurses, from Apple's distribution.
#
# Apple ship libncurses.5.4.dylib and its friends -- form, menu, panel --
# with the headers in usr/include at the top level.  The SDK carries the
# stubs and the headers; the libraries themselves belong to the OS, so
# nothing is installed into the release tree but headers.
#
# The drop is an Xcode project wrapped around upstream ncurses 6.0, and
# the xcodeproj drives a build this tree cannot run.  Underneath it the
# upstream tree at ncurses/ is ordinary autoconf, which is what gets
# built -- the same thing Apple's own configure.sh does.
#
# The flags are theirs, taken from that script rather than chosen here.
# --with-abi-version=5.4 is the one that shows: it is why the library is
# libncurses.5.4 and why the SDK's stubs carry that name, thirty years
# after 5.4 was current.
# The drop ships configure mode 644 -- Apple's own script runs it as
# "sh configure", so it never needed the bit.  Set it in the copy; the
# submodule is left alone.
P_PREPARE=	chmod +x ncurses/configure

# Built in tree, inside ncurses/, which is what Apple's configure.sh does --
# "cd ncurses && sh configure".  It matters: configure regenerates
# include/curses.head from curses.h.in, and generate-syms.py below looks for
# that file at the path an in-tree build puts it.  Out of tree the header
# lands somewhere the script does not look, and the marker survives into
# curses.h, where it is a syntax error.
P_OBJDIR=	${P_WORKDIR}/src/ncurses

P_CONFIGURE=	ncurses/configure

# The second half of Apple's configure.sh.  configure leaves @@ABIDECLS@@ in
# the generated curses.head; this fills it in with the __asm declarations
# that give their libncurses its symbol versioning.  Without it the SDK's
# curses.h does not compile.
P_POST_CONFIGURE=	python3 ../generate-syms.py

P_CONFIGURE_ARGS=	--with-shared \
			--without-normal \
			--without-debug \
			--without-cxx-binding \
			--without-cxx \
			--enable-termcap \
			--enable-widec \
			--enable-ext-colors \
			--with-abi-version=5.4 \
			--mandir=/usr/share/man \
			--datarootdir=/usr/share

# Only the headers are wanted, and only the headers are built.  The
# library build does not survive this make -- ncurses 6.0's generated
# Makefile gives "target file `depend' has both : and :: entries" under
# GNU make 3.81, which is what Apple's xcodeproj build sidesteps by never
# running it -- and there is nothing here that wants the library anyway.
# install.includes is upstream's own rule for exactly this set, and what
# it installs is exactly what Apple ship: the ten obvious ones plus tic.h,
# nc_tparm.h and term_entry.h.
#
# The headers are generated, not shipped: curses.h comes from curses.head
# and Caps by way of the include/ makefile, term.h from MKterm.h.awk,
# ncurses_dll.h from the configure run.  They exist only after a build,
# which is why there is one at all.
P_MAKE_ARGS=	install.includes DESTDIR=${P_OBJDIR}/dest
P_NOSTAGE=	yes

# No programs: P_PROGS defaults to bin/<port name>, and this one installs
# nothing but headers.
P_PROGS=

# ncurses.h is the name most code includes and upstream does not create it
# in this configuration; Apple's SDK carries it as a link to curses.h, so
# this does the same.
P_POST_BUILD=	ln -sfn curses.h dest/usr/include/ncurses.h

SDKS=		Platforms/MacOSX.platform/Developer/SDKs

P_RELEASE_MERGE=	dest/usr/include ${SDKS}/MacOSX.sdk/usr/include \
			dest/usr/include ${SDKS}/MacOSX.Internal.sdk/usr/include
