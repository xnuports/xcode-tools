# PlistBuddy -- CoreFoundation does all the property-list work, per the
# submodule's own Makefile (LDADD = -framework CoreFoundation).
T_LDADD+=	-framework CoreFoundation
