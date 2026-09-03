# git.
#
# Xcode ships it at Developer/usr/bin/git, and the checkout here is
# v2.50.1, which is the version Apple's build of it reports.  So this
# is a parity tool and goes to usr/bin rather than usr/local.
#
# Driven straight through its Makefile: git ships one that takes a
# prefix and installs, and its configure is generated rather than
# checked in.
P_BUILDSYS=	make
P_MAKE_ARGS=	prefix=${P_PREFIX} \
		NO_GETTEXT=1 \
		NO_TCLTK=1
P_PROGS=	bin/git
