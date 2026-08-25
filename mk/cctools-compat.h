/*
 * mk/cctools-compat.h
 *
 * Force-included into every cctools translation unit (see
 * mk/with-cctools.mk).  Supplies definitions that cctools' sources
 * reference but that neither the public SDK nor cctools' own bundled
 * headers actually define.
 *
 * This file is ours (BSD-3-Clause); it adds no Apple code.
 */

#ifndef XCODE_TOOLS_CCTOOLS_COMPAT_H
#define XCODE_TOOLS_CCTOOLS_COMPAT_H

/*
 * Deliberately includes nothing.  cctools' own
 * include/mach/machine-cctools.h reuses the SDK's _MACH_MACHINE_H_
 * guard, so pulling in <mach/machine.h> here would turn cctools'
 * overrides into a no-op and lose the definitions only they provide.
 * Macros expand at the use site, so no declaration is needed for the
 * cpu_type_t casts below.
 */

/*
 * CPU_TYPE_RISCV32.
 *
 * cctools/libstuff/reloc.c switches on CPU_TYPE_RISCV32, but the
 * constant is defined nowhere in the open-source drop: cctools'
 * own include/mach/machine-cctools.h carries the section comment
 *
 *	/ *  RISC-V subtypes  * /
 *
 * with the definitions beneath it stripped, and the public SDK's
 * <mach/machine.h> stops at CPU_TYPE_POWERPC64.  The value is recovered
 * from LLVM, which we carry as a submodule and which is the same
 * definition the linker and the assembler agree on:
 *
 *	src/llvm-project/llvm/include/llvm/BinaryFormat/MachO.h
 *		CPU_TYPE_RISCV = 24
 *		CPU_SUBTYPE_RISCV_ALL = 0
 *
 * riscv32 is the 32-bit variant, so it carries no CPU_ARCH_ABI64 bit and
 * is simply 24.
 *
 * Note that Apple themselves treat riscv32 as absent from this
 * toolchain: ld64's src/create_configure emits
 * "#define SUPPORT_ARCH_riscv32 0" whenever TOOLCHAIN_INSTALL_DIR
 * matches XcodeDefault, which is the toolchain we build.  These
 * definitions therefore exist to let the sources compile, not to claim
 * working RISC-V support.
 */
#ifndef CPU_TYPE_RISCV32
#define CPU_TYPE_RISCV32	((cpu_type_t) 24)
#endif

#ifndef CPU_SUBTYPE_RISCV_ALL
#define CPU_SUBTYPE_RISCV_ALL	((cpu_subtype_t) 0)
#endif

#endif /* XCODE_TOOLS_CCTOOLS_COMPAT_H */
