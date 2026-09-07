# macho -- a Mach-O parser and dumper: load commands, segments, sections,
# symbols, and the images inside a dyld shared cache.
#
# The toolchain, not usr/local: by the rule mk/ports.mk states for
# src/extras, a tool that works on Mach-O belongs beside clang and ld.
#
# A port rather than a mk/tool.mk entry, which is what everything else
# imported here is.  The reason is a name: the project's own XS++ library
# and its lib-macho each have a ToString.cpp, and mk/tool.mk keys objects
# by basename, so building the sources directly would have one object
# overwrite the other.  The project already knows how to keep them apart,
# so it is left to do that.
#
# The four overrides are all command line, not edits to the project:
#
#	ONLY_ACTIVE_ARCH  this tree builds for the machine it runs on;
#			the project would otherwise build universal and
#			carry an x86 half nothing here runs.
#	CODE_SIGNING_*  the project asks for its author's "Mac Development"
#			certificate, which no one else has.
#	GCC_TREAT_WARNINGS_AS_ERRORS  XS++ uses std::wstring_convert, which
#			the current libc++ deprecates.  Imported sources are
#			exempt from -Werror everywhere else in this tree
#			(mk/xcodetools.sys.mk); this is the same exemption.
P_BUILDSYS=	make
P_COPY=		no
P_NOSTAGE=	yes
P_OBJDIR=	${P_WORKDIR}/dd/Build/Products

P_MAKE=		xcodebuild
P_MAKE_ARGS=	-project ${TOP}/src/extras/macho/macho.xcodeproj \
		-scheme macho \
		-configuration Release \
		-derivedDataPath ${P_WORKDIR}/dd \
		ONLY_ACTIVE_ARCH=YES \
		CODE_SIGNING_ALLOWED=NO \
		CODE_SIGNING_REQUIRED=NO \
		CODE_SIGN_IDENTITY= \
		DEVELOPMENT_TEAM= \
		GCC_TREAT_WARNINGS_AS_ERRORS=NO \
		build

P_PROGS=	Release/macho
