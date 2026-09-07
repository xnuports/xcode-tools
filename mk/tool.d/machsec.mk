# machsec -- reports which hardening a Mach-O binary was built with: PIE,
# stack canaries, ARC, code signing, and the ones that can only be seen in
# the instruction stream, which is what it disassembles __TEXT for.
#
# Capstone is a port here (mk/port.d/capstone.mk), so it is named from
# usr/local rather than looked for on the system; the submodule's Makefile
# says -lcapstone and leaves finding it to the compiler's defaults, which
# would pick up whatever Homebrew has.
T_CFLAGS+=	-I${TOP}/build/release/usr/local/include
T_LDADD+=	${TOP}/build/release/usr/local/lib/libcapstone.a
