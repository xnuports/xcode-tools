# open -- /usr/bin/open.  The submodule carries both a macOS and an iOS
# implementation in the same directory, so the sources are named rather
# than globbed: ios_open.m is the other one and must not be compiled here.
#
# No -fobjc-arc.  This is the one AKCmds tool written for manual retain
# and release; it asks for -fblocks instead, and building it with ARC
# fails outright.
T_SRCS=		open.m header_search.m utils.m
T_CFLAGS+=	-fblocks
T_LDADD+=	-framework Foundation
T_LDADD+=	-framework CoreFoundation
T_LDADD+=	-framework AppKit
T_LDADD+=	-framework ApplicationServices
T_LDADD+=	-lobjc
