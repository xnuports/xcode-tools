# gnumake -- GNU Make, as Apple ships it (Developer/usr/bin/make).
#
# job.c and remake.c call general_vpath_search() and
# allocated_vpath_expand_for_file(), which live in next.c -- an
# Apple/NeXT-specific source that the autoconf Makefile never lists,
# because Apple builds this project from their own project file rather
# than from configure.  Without it the link fails on those two symbols.
#
# Append next.c to the tail of both the source list and the object list.
P_PREPARE=	sed -i.bak \
		-e 's|^	hash\.c$$|	hash.c next.c|' \
		-e 's|^	hash\$$U\.\$$(OBJEXT)$$|	hash$$U.$$(OBJEXT) next$$U.$$(OBJEXT)|' \
		Makefile.in

# The port installs its binary as "make"; Apple ships both names in
# Developer/usr/bin, so gnumake is hardlinked beside it.
P_PROGS=	bin/make
P_LINKS=	gnumake
