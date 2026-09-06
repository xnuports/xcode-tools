# xcdebug -- drives Xcode through its scripting vocabulary, so it needs
# Foundation for NSAppleScript.  Objective-C: tool.mk compiles anything it
# is given by name with ${CC}, and clang takes .m from the suffix.
T_SRCS=		xcdebug.m
T_LDADD+=	-framework Foundation
