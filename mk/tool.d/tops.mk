# tops -- the scripted find-and-replace tool.  Nine sources and nothing
# but Foundation; the parser and the class-hierarchy walker are its own.
T_SRCS=		main.m ClassHierarchy.m Common.m Find.m Replace.m \
		TokenizedInput.m TokenizedTopsInput.m Tops.m TopsParser.m
T_CFLAGS+=	-fobjc-arc
T_LDADD+=	-framework Foundation
T_LDADD+=	-lobjc
