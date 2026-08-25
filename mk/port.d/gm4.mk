# gm4 -- GNU M4.
#
# Its bundled gnulib generates lib/alloca.h and lib/getopt.h from
# lib/alloca_.h and lib/getopt_.h, but Apple's drop ships the generated
# headers without the templates, so the build stops at "No rule to make
# target 'alloca_.h'".  Restore them from the headers that did ship.
P_PREPARE=	${TOP}/mk/scripts/gnulib-restore-templates.sh
