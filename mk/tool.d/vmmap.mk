# vmmap -- virtual memory map inspector.
#
# Apple ships vmmap in both /usr/bin and Developer/usr/bin but has never
# released its source, so this is a third-party implementation (MIT,
# Trung Nguyen) written for Darling.  It compiles unmodified on macOS:
# the Mach VM interfaces it uses -- mach_vm_region_recurse and friends --
# are the real ones here rather than Darling's reimplementation.
T_CXXFLAGS+=	-std=c++11
T_LDADD+=	-framework CoreFoundation

# Not verified against a live process: vmmap needs task_for_pid, which
# macOS grants only to root or to a binary carrying the debugger
# entitlement.  Apple's copy is codesigned for it; ours is not, so it
# reports a privilege error instead of a memory map unless run under
# sudo.  Its error mapping is also looser than Apple's -- a denied
# task_for_pid sometimes surfaces as "no longer appears to be running"
# rather than the privilege message -- which is worth tightening when
# someone can test it with privileges.
