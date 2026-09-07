/*
 * atos -- turn addresses in a binary into symbols.
 *
 * Apple's is a thin front end: 319 KB linking CoreSymbolicationDT and
 * SymbolicationDT, two private frameworks that do the actual work.  That is
 * the shape xccov and xcresulttool had -- the framework is their factoring,
 * and behind it is a file format anyone can read.  Here that format is
 * DWARF in a Mach-O or a dSYM, and llvm-project, which this tree already
 * builds, reads it.
 *
 * The alternative was libdwarf, which is LGPL-2.1: a static dependency this
 * BSD-3-Clause tree should not take on when an Apache-2.0 reader is already
 * on hand.  atosl, the other candidate, is archived since 2015, knows no
 * x86_64, and wants libiberty out of binutils for demangling alone.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace llvm;

namespace {

struct Options {
	std::string	binary;		/* -o */
	std::string	addrfile;	/* -f */
	std::string	arch;		/* -arch */
	std::string	delimiter;	/* -d, empty being a blank line */
	uint64_t	slide = 0;	/* -s */
	uint64_t	load_address = 0;	/* -l */
	bool		have_load = false;
	bool		offsets = false;	/* --offset */
	bool		full_path = false;	/* --fullPath */
	bool		inline_frames = false;	/* -i */
	bool		print_header = false;	/* -printHeader */
	bool		dedup = false;		/* --dedup */
};

void
usage(FILE *out, bool lead_blank)
{
	if (lead_blank)
		fprintf(out, "\n");
	fprintf(out, "Usage: atos [-p pid] [-o executable/dSYM] "
	    "[-f file-of-input-addresses] [-s slide | -l loadAddress | "
	    "-textExecAddress addr | -offset] [-arch architecture] "
	    "[-printHeader] [-fullPath] [-inlineFrames] [-dedup] "
	    "[-d delimiter] [address ...]\n\n");
	fprintf(out, "        -d/--delimiter     delimiter when outputting inline "
	    "frames. Defaults to newline.\n");
	fprintf(out, "        --fullPath         show full path to source file\n");
	fprintf(out, "        -i/--inlineFrames  display inlined functions\n");
	fprintf(out, "        --offset           treat all following addresses as "
	    "offsets into the binary\n");
	fprintf(out, "        --dedup            show all variants for "
	    "<deduplicated_symbol> at address\n");
}

/*
 * How Apple print one frame.  A resolved address with line information is
 * "name (in image) (file:line)"; with only a symbol it is "name (in image)
 * + offset"; an address inside the image that matched nothing is the
 * address itself, zero padded, followed by the image; and an address that
 * is not in the image at all is echoed back exactly as it was typed.
 */
/*
 * __mh_execute_header covers the Mach header rather than any code, so an
 * address that lands on it has not been symbolicated at all.  Apple report
 * those as an address in the image; so does this.
 */
/*
 * What this tool needs to know about the image, gathered once so that no
 * LLVM object has to be kept alive past the scan.
 */
struct image_info {
	uint64_t	base = 0;	/* first mapped segment */
	uint64_t	low = 0;
	uint64_t	high = 0;
	bool		readable = false;
	/* Branch islands: where each run of stubs starts, and their names. */
	struct stub {
		uint64_t	addr;
		uint64_t	size;
		std::string	name;
	};
	std::vector<stub>	stubs;
	/* Where each function begins, from LC_FUNCTION_STARTS. */
	std::vector<uint64_t>	function_starts;
	/* The range those starts describe: __text, and nothing else. */
	uint64_t		text_low = 0;
	uint64_t		text_high = 0;
};

/*
 * Whether the image can say how far into a function an address is.  Asked
 * because the two modes give up at different points: without --inlineFrames
 * an unnamed address still prints as an address in the image, and with it
 * Apple print an empty line unless there is at least an offset to give.
 */
bool
has_function_offset(const image_info &info, uint64_t vmaddr)
{
	if (vmaddr < info.text_low || vmaddr >= info.text_high)
		return (false);
	return (!info.function_starts.empty() &&
	    info.function_starts.front() <= vmaddr);
}

/*
 * A branch island, if the address is in one.  Returns false when it is not,
 * so the caller can fall through to whatever else it has.
 */
bool
print_stub(const image_info &info, uint64_t vmaddr, const std::string &image)
{
	for (const auto &st : info.stubs) {
		if (vmaddr < st.addr || vmaddr >= st.addr + st.size)
			continue;
		printf("%s (in %s) + %" PRIu64 "\n", st.name.c_str(),
		    image.c_str(), vmaddr - st.addr);
		return (true);
	}
	return (false);
}

/*
 * All this tool can say when it has no name: the address, as an address in
 * the image when it falls inside one, and otherwise echoed back exactly as
 * it was typed.  Every path that gives up ends here, so that a stripped
 * binary -- where the symbolizer returns nothing at all rather than an
 * unnamed frame -- still reports the image.
 */
void
print_bare(uint64_t addr, uint64_t vmaddr, bool in_image,
    const image_info &info, const std::string &image,
    const std::string &as_typed)
{
	if (!in_image) {
		printf("%s\n", as_typed.c_str());
		return;
	}
	printf("0x%0*" PRIx64 " (in %s)", addr > 0xffffffffULL ? 16 : 8, addr,
	    image.c_str());
	/*
	 * The greatest function start at or below the address: how far into
	 * a function it is, even though nothing names that function.  Only
	 * within __text -- past its end there is no function to be inside,
	 * and Apple print no offset there either.
	 */
	if (vmaddr >= info.text_low && vmaddr < info.text_high) {
		auto it = std::upper_bound(info.function_starts.begin(),
		    info.function_starts.end(), vmaddr);

		if (it != info.function_starts.begin()) {
			--it;
			printf(" + %" PRIu64, vmaddr - *it);
		}
	}
	printf("\n");
}

bool
named(const DILineInfo &info)
{
	return (!info.FunctionName.empty() &&
	    info.FunctionName != DILineInfo::BadString &&
	    info.FunctionName != "_mh_execute_header" &&
	    info.FunctionName != "__mh_execute_header" &&
	    info.FunctionName != "_mh_dylib_header" &&
	    info.FunctionName != "_mh_bundle_header");
}

void
print_frame(const DILineInfo &info, uint64_t addr, uint64_t vmaddr,
    bool in_image, const image_info *scan, const std::string &image,
    const Options &opt, const std::string &as_typed)
{
	bool have_name = named(info);
	bool have_file = !info.FileName.empty() &&
	    info.FileName != DILineInfo::BadString && info.Line != 0;

	if (!have_name) {
		print_bare(addr, vmaddr, in_image, *scan, image, as_typed);
		return;
	}
	if (have_file) {
		StringRef file(info.FileName);

		if (!opt.full_path)
			file = sys::path::filename(file);
		printf("%s (in %s) (%s:%u)\n", info.FunctionName.c_str(),
		    image.c_str(), file.str().c_str(), info.Line);
		return;
	}
	printf("%s (in %s) + %" PRIu64 "\n", info.FunctionName.c_str(),
	    image.c_str(), addr - info.StartAddress.value_or(addr));
}

/*
 * The address LLVM wants for a Mach-O is a virtual one, so an offset into
 * the image has to be put back on top of where the image says it begins --
 * the first segment with a non-zero size, __TEXT in everything this will be
 * pointed at.
 */
/*
 * What this machine calls its architecture in the message Apple print when
 * a file yields no symbols.  LLVM's triple can spell it aarch64, which is
 * not a name any Mach-O header uses.
 */
std::string
host_arch(void)
{
	std::string a = Triple(sys::getDefaultTargetTriple()).getArchName().str();

	if (a == "aarch64")
		return ("arm64");
	return (a);
}

/*
 * LC_FUNCTION_STARTS is a run of ULEB128 deltas from the start of the first
 * segment, and it survives stripping.  It is what lets a symbolicator say
 * "this address is 696 bytes into some function" for a binary with no
 * symbol table left, which is most of what a crash log from /bin or
 * /usr/lib contains.
 */
void
scan_function_starts(const object::MachOObjectFile &macho, image_info &info)
{
	for (const auto &cmd : macho.load_commands()) {
		if (cmd.C.cmd != MachO::LC_FUNCTION_STARTS)
			continue;

		MachO::linkedit_data_command led =
		    macho.getLinkeditDataLoadCommand(cmd);
		StringRef buf = macho.getData();

		if (led.dataoff + led.datasize > buf.size())
			return;

		const uint8_t *p = (const uint8_t *)buf.data() + led.dataoff;
		const uint8_t *end = p + led.datasize;
		uint64_t addr = info.base;

		while (p < end) {
			uint64_t delta = 0;
			unsigned shift = 0;

			for (;;) {
				if (p >= end)
					return;

				uint8_t byte = *p++;

				delta |= (uint64_t)(byte & 0x7f) << shift;
				shift += 7;
				if ((byte & 0x80) == 0)
					break;
			}
			if (delta == 0)	/* the terminator */
				break;
			addr += delta;
			info.function_starts.push_back(addr);
		}
		return;
	}
}

/*
 * A branch island in __TEXT,__stubs has no symbol of its own; it stands in
 * for one the image imports.  Apple name those DYLD-STUB$$<symbol>, which
 * is often the most useful thing a backtrace through a stub can say, so the
 * names are recovered the way dyld finds them: the section records where its
 * run of indirect symbol table entries starts and how big each stub is, and
 * the entry names the symbol.
 */
void
scan_stubs(const object::MachOObjectFile &macho, image_info &info)
{
	MachO::dysymtab_command dysym = macho.getDysymtabLoadCommand();

	for (const object::SectionRef &sec : macho.sections()) {
		object::DataRefImpl impl = sec.getRawDataRefImpl();
		uint32_t flags, reserved1, reserved2;
		uint64_t addr, size;

		if (macho.is64Bit()) {
			MachO::section_64 s = macho.getSection64(impl);

			flags = s.flags;
			reserved1 = s.reserved1;
			reserved2 = s.reserved2;
			addr = s.addr;
			size = s.size;
		} else {
			MachO::section s = macho.getSection(impl);

			flags = s.flags;
			reserved1 = s.reserved1;
			reserved2 = s.reserved2;
			addr = s.addr;
			size = s.size;
		}
		if ((flags & MachO::SECTION_TYPE) != MachO::S_SYMBOL_STUBS ||
		    reserved2 == 0)
			continue;

		for (uint64_t off = 0; off + reserved2 <= size;
		    off += reserved2) {
			uint32_t idx = reserved1 +
			    (uint32_t)(off / reserved2);
			uint32_t sym;

			if (idx >= dysym.nindirectsyms)
				break;
			sym = macho.getIndirectSymbolTableEntry(dysym, idx);
			if (sym == MachO::INDIRECT_SYMBOL_ABS ||
			    sym == MachO::INDIRECT_SYMBOL_LOCAL)
				continue;
			/*
			 * Indexed rather than searched.  A linear scan of the
			 * symbol table per stub is quadratic, and it misses
			 * the imports of a stripped binary, which is exactly
			 * the case stubs matter for.
			 */
			object::SymbolRef sr(
			    macho.getSymbolByIndex(sym)->getRawDataRefImpl(),
			    &macho);
			auto n = sr.getName();

			if (!n) {
				consumeError(n.takeError());
				continue;
			}
			StringRef name = *n;

			/* Stored with the C leading underscore. */
			if (name.starts_with("_"))
				name = name.drop_front();
			info.stubs.push_back({ addr + off, reserved2,
			    ("DYLD-STUB$$" + name).str() });
		}
	}
}

/*
 * The segments the image maps, ignoring __PAGEZERO, which reserves the low
 * addresses and contains nothing.  Apple distinguish an address inside the
 * image from one outside it: the first prints as an address in the image,
 * the second is echoed back as it was typed.
 *
 * A universal file is opened at the slice asked for, or the one this
 * machine runs.  /bin and /usr/lib are still full of two-architecture
 * files, and picking the wrong half puts every address out of range.
 */
image_info
scan_image(const std::string &path, const std::string &arch)
{
	image_info info;
	auto bin = object::createBinary(path);

	if (!bin) {
		consumeError(bin.takeError());
		return (info);
	}

	const object::MachOObjectFile *macho = nullptr;
	std::unique_ptr<object::ObjectFile> slice;

	if (auto *fat = dyn_cast<object::MachOUniversalBinary>(
	    bin->getBinary())) {
		/*
		 * The slice is chosen by parsed architecture rather than by
		 * the name in the fat header, so that arm64 answers for
		 * arm64e -- a machine that runs one runs the other, and
		 * Apple's tool does not make the caller spell out which.
		 * Comparing the names would not: LLVM spells the host arm64
		 * and the header spells the slice arm64e.
		 */
		Triple::ArchType want = arch.empty() ?
		    Triple(sys::getDefaultTargetTriple()).getArch() :
		    Triple(arch + "--").getArch();

		for (auto obj : fat->objects()) {
			auto o = obj.getAsObjectFile();

			if (!o) {
				consumeError(o.takeError());
				continue;
			}
			bool exact = arch.empty() ? false :
			    std::string(obj.getArchFlagName()) == arch;

			if (exact || (*o)->makeTriple().getArch() == want) {
				slice = std::move(*o);
				if (exact || arch.empty())
					break;
			}
			if (slice == nullptr)
				slice = std::move(*o);
		}
		macho = dyn_cast_or_null<object::MachOObjectFile>(slice.get());
	} else
		macho = dyn_cast<object::MachOObjectFile>(bin->getBinary());

	if (macho == nullptr)
		return (info);
	info.readable = true;

	for (const auto &cmd : macho->load_commands()) {
		uint64_t vmaddr, vmsize;
		const char *name;

		if (cmd.C.cmd == MachO::LC_SEGMENT_64) {
			auto seg = macho->getSegment64LoadCommand(cmd);

			vmaddr = seg.vmaddr;
			vmsize = seg.vmsize;
			name = seg.segname;
		} else if (cmd.C.cmd == MachO::LC_SEGMENT) {
			auto seg = macho->getSegmentLoadCommand(cmd);

			vmaddr = seg.vmaddr;
			vmsize = seg.vmsize;
			name = seg.segname;
		} else
			continue;
		if (vmsize == 0 || strncmp(name, "__PAGEZERO", 16) == 0)
			continue;
		if (info.high == 0) {
			info.base = vmaddr;
			info.low = vmaddr;
		}
		if (vmaddr < info.low)
			info.low = vmaddr;
		if (vmaddr + vmsize > info.high)
			info.high = vmaddr + vmsize;
	}
	for (const object::SectionRef &sec : macho->sections()) {
		auto n = sec.getName();

		if (!n) {
			consumeError(n.takeError());
			continue;
		}
		if (*n == "__text") {
			info.text_low = sec.getAddress();
			info.text_high = sec.getAddress() + sec.getSize();
			break;
		}
	}
	scan_stubs(*macho, info);
	scan_function_starts(*macho, info);
	return (info);
}

bool
parse_address(const char *s, uint64_t *out)
{
	char *end = nullptr;
	unsigned long long v;

	errno = 0;
	v = strtoull(s, &end, 0);
	if (end == s || *end != '\0' || errno != 0)
		return (false);
	*out = (uint64_t)v;
	return (true);
}

} /* namespace */

int
main(int argc, char *argv[])
{
	Options opt;
	std::vector<std::string> addresses;
	int i;

	for (i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto next = [&](const char *what) -> const char * {
			if (i + 1 >= argc) {
				fprintf(stderr, "atos: %s requires an "
				    "argument\n", what);
				exit(1);
			}
			return (argv[++i]);
		};

		if (a == "-o" || a == "--object")
			opt.binary = next("-o");
		else if (a == "-f" || a == "--file")
			opt.addrfile = next("-f");
		else if (a == "-arch" || a == "--arch")
			opt.arch = next("-arch");
		else if (a == "-d" || a == "--delimiter")
			opt.delimiter = next("-d");
		else if (a == "-s" || a == "--slide") {
			if (!parse_address(next("-s"), &opt.slide)) {
				fprintf(stderr, "atos: bad slide\n");
				return (1);
			}
		} else if (a == "-l" || a == "--loadAddress") {
			if (!parse_address(next("-l"), &opt.load_address)) {
				fprintf(stderr, "atos: bad load address\n");
				return (1);
			}
			opt.have_load = true;
		} else if (a == "--offset" || a == "-offset")
			opt.offsets = true;
		else if (a == "--fullPath" || a == "-fullPath")
			opt.full_path = true;
		else if (a == "-i" || a == "--inlineFrames" ||
		    a == "-inlineFrames")
			opt.inline_frames = true;
		else if (a == "-printHeader" || a == "--printHeader")
			opt.print_header = true;
		else if (a == "--dedup" || a == "-dedup")
			opt.dedup = true;
		else if (a == "-p" || a == "--pid") {
			/*
			 * Symbolicating a running process means reading its
			 * task port and its loaded image list, which is what
			 * CoreSymbolication does and what nothing outside
			 * Apple can do without it.  Better to say so than to
			 * answer with something plausible and wrong.
			 */
			fprintf(stderr, "atos: -p is not implemented in this "
			    "build: symbolicating a live process needs the "
			    "private symbolication frameworks Apple's links.  "
			    "Use -o with the binary or its dSYM.\n");
			return (1);
		} else if (a == "-h" || a == "--help") {
			usage(stdout, false);
			return (0);
		} else
			addresses.push_back(a);
	}

	if (opt.binary.empty()) {
		fprintf(stderr, "[invalid usage]: no processes or executables "
		    "specified\n");
		usage(stderr, true);
		return (1);
	}

	if (!opt.addrfile.empty()) {
		FILE *f = fopen(opt.addrfile.c_str(), "r");
		char line[256];

		if (f == nullptr) {
			fprintf(stderr, "atos: cannot open %s\n",
			    opt.addrfile.c_str());
			return (1);
		}
		while (fgets(line, sizeof(line), f) != nullptr) {
			char *p = line;

			while (*p == ' ' || *p == '\t')
				p++;
			p[strcspn(p, "\r\n")] = '\0';
			if (*p != '\0')
				addresses.push_back(p);
		}
		fclose(f);
	}
	/* With no addresses on the command line, they come from stdin. */
	if (addresses.empty() && opt.addrfile.empty()) {
		char line[256];

		while (fgets(line, sizeof(line), stdin) != nullptr) {
			char *p = line;

			while (*p == ' ' || *p == '\t')
				p++;
			p[strcspn(p, "\r\n")] = '\0';
			if (*p != '\0')
				addresses.push_back(p);
		}
	}

	symbolize::LLVMSymbolizer::Options sopts;
	sopts.PrintFunctions = DILineInfoSpecifier::FunctionNameKind::LinkageName;
	sopts.UseSymbolTable = true;
	sopts.Demangle = true;
	if (!opt.arch.empty())
		sopts.DefaultArch = opt.arch;
	symbolize::LLVMSymbolizer symbolizer(sopts);

	std::string image = sys::path::filename(opt.binary).str();
	/*
	 * A dSYM is named for its binary: the image Apple print is the
	 * executable's name, not the bundle's.
	 */
	if (image.size() > 5 && image.compare(image.size() - 5, 5, ".dSYM") == 0)
		image = image.substr(0, image.size() - 5);

	image_info info = scan_image(opt.binary, opt.arch);

	if (!info.readable) {
		fprintf(stderr, "atos cannot load symbols for the file %s for "
		    "architecture %s.\n", opt.binary.c_str(),
		    opt.arch.empty() ? host_arch().c_str() :
		    opt.arch.c_str());
		return (1);
	}
	uint64_t base = info.base;

	for (const std::string &as_typed : addresses) {
		uint64_t addr, module_offset;

		if (!parse_address(as_typed.c_str(), &addr)) {
			printf("%s\n", as_typed.c_str());
			continue;
		}
		/*
		 * --offset says the address already is one; otherwise a slide
		 * or a load address says where the image was mapped, and what
		 * is left is the offset into it.
		 */
		if (opt.offsets)
			module_offset = base + addr;
		else if (opt.have_load)
			module_offset = base + (addr - opt.load_address);
		else if (opt.slide != 0)
			module_offset = addr - opt.slide;
		else
			module_offset = addr;

		if (addr == 0) {
			printf("%s\n", as_typed.c_str());
			if (opt.inline_frames)
				printf("%s\n", opt.delimiter.c_str());
			continue;
		}

		bool in_image = info.high != 0 &&
		    module_offset >= info.low && module_offset < info.high;

		object::SectionedAddress sa;
		sa.Address = module_offset;
		sa.SectionIndex = object::SectionedAddress::UndefSection;

		/*
		 * Always ask for the inlined chain, even when only one line
		 * is wanted.  Asking for a single answer gives the innermost
		 * frame -- the body that was inlined -- where Apple report
		 * the function the address belongs to.  The chain runs
		 * innermost first, so that function is its last entry.
		 */
		auto res = symbolizer.symbolizeInlinedCode(opt.binary, sa);

		/*
		 * A stripped image yields no frames at all rather than an
		 * unnamed one, so the stub table is consulted here too --
		 * which is the case it matters most for, since a stub is all
		 * such an image can be said to contain.
		 */
		if (!res || res->getNumberOfFrames() == 0) {
			if (!res)
				consumeError(res.takeError());
			if (!print_stub(info, module_offset, image)) {
				if (opt.inline_frames && in_image &&
				    !has_function_offset(info, module_offset))
					printf("\n");
				else
					print_bare(addr, module_offset,
					    in_image, info, image, as_typed);
			}
			if (opt.inline_frames)
				printf("%s\n", opt.delimiter.c_str());
			continue;
		}
		uint32_t n = res->getNumberOfFrames();
		/*
		 * What to print, decided once: the debug information if it
		 * covers the address, a stub name if the address is a branch
		 * island, and otherwise the address -- as an address in the
		 * image, or echoed back when it is not in the image at all.
		 * Deciding it here rather than in each branch is what keeps
		 * the inline-mode delimiter from being skipped.
		 */
		bool have_dwarf = named(res->getFrame(0));
		if (have_dwarf) {
			if (opt.inline_frames) {
				for (uint32_t k = 0; k < n; k++)
					print_frame(res->getFrame(k), addr,
					    module_offset, in_image,
					    &info, image, opt, as_typed);
			} else
				print_frame(res->getFrame(n - 1), addr,
				    module_offset, in_image,
				    &info, image, opt, as_typed);
		} else if (print_stub(info, module_offset, image)) {
			/* said by print_stub */
		} else if (opt.inline_frames) {
			/*
			 * Asked for the chain, Apple give up sooner: an
			 * address inside the image that no debug information
			 * covers gets an empty line, unless the function
			 * starts can at least say how far into a function it
			 * is.
			 */
			if (in_image && has_function_offset(info,
			    module_offset))
				print_bare(addr, module_offset, in_image, info,
				    image, as_typed);
			else if (in_image)
				printf("\n");
			else
				printf("%s\n", as_typed.c_str());
		} else
			print_frame(res->getFrame(n - 1), addr, module_offset,
			    in_image, &info, image, opt, as_typed);

		if (opt.inline_frames)
			printf("%s\n", opt.delimiter.c_str());
	}
	return (0);
}
