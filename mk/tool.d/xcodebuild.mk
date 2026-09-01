# xcodebuild -- build orchestration.
T_SRCS=	build.c ini.c project.c settings.c xcodebuild.c
.include "${TOP}/mk/with-devpath.mk"
.include "${TOP}/mk/with-sdkpath.mk"

# Apple's xcodebuild parses project files with CoreFoundation; a
# .pbxproj is an OpenStep property list, which CF reads directly.
T_LDADD+=	-framework CoreFoundation
