xcode-tools
===========

An open-source reimplementation of Apple's Xcode command-line developer tools.

The goal is a `build/release/` tree that can stand in for
`/Applications/Xcode.app/Contents/Developer` — the same layout, the same tool
names, the same behaviour — built entirely from open source. Everything follows
Apple's own releases (APSL/GPL/BSD/Apache as applicable), with BSD-licensed
reimplementations of our own where Apple has published nothing.

## State

A working C and C++ toolchain builds and links real programs today:

```sh
clang -isysroot $SDK -arch arm64 -c hello.c -o hello.o
ld -o hello hello.o -lSystem -syslibroot $SDK -arch arm64 \
   -platform_version macos 26.0 26.0
./hello
```

— our clang, our ld64, our libtapi reading the SDK's `.tbd` stubs, and the
result inspectable with our own `otool-classic`, `nm-classic` and `size-classic`.

`xcrun` and `xcodebuild` read Apple's layout directly: they find SDKs inside
platform bundles, parse `SDKSettings.plist` in binary, XML or NextSTEP form, and
work against a stock Xcode with no configuration at all. `xcrun --show-sdk-path`,
`--show-sdk-version` and `--find` match Apple's output exactly when pointed at
one.

**48 programs** build: 42 compiled directly, 6 through their own build systems.

| Where | What |
|---|---|
| `usr/bin` | our 10 reimplementations, plus headerdoc, pngcrush, `xml2man`, `resolveLinks`, `make`/`gnumake`, `bsdmake`, `bmake` |
| `Toolchains/XcodeDefault.xctoolchain/usr/bin` | `clang`/`clang++`/`cc`/`c++`/`cpp`, `ld`, the cctools set, the llvm-* tools, `dsymutil`, developer_cmds, `flex`, `gperf` |
| `Toolchains/XcodeDefault.xctoolchain/usr/lib` | `libtapi.dylib`, clang's resource directory |
| `usr/libexec` | `PlistBuddy` |
| `usr/local/bin` | `bmake`, `bsdmake` |
| `Makefiles/` | `CoreOS` and `pb_makefiles` build fragments |
| `Platforms/`, `Toolchains/` | emitted `.sdk` and `.xctoolchain` bundle metadata |

`bmake` and `bsdmake` sit in `usr/local/bin` rather than `usr/bin` because
Xcode ships neither — they are ours, and building them removes the last
external build dependency beyond a C compiler. Both need their system rules
pointed at explicitly, and the two differ in how:

```sh
MAKESYSPATH=<developer>/usr/local/share/mk bmake ...
bsdmake -m <developer>/usr/local/share/bsdmake/mk ...
```

`Makefiles/CoreOS` is generated the way Apple's own rules generate it —
`Standard/Commands.make` and `Variables.make` come from the `.in` templates
through `unifdef`, using the `unifdef` this tree builds. The result is
byte-identical to Apple's.

The SDK is a skeleton: it carries settings but no headers or libraries yet, so
building against *our* SDK does not work — point `-isysroot` at Apple's for now.
Populating it is the next major piece.

`vmmap` is a third-party implementation (MIT, written for Darling) of a tool
Apple ships but has never open-sourced. It compiles unmodified here — the Mach
VM interfaces it uses are the real ones on macOS.

### Our own reimplementations

`codesign` (ad-hoc and certificate signing; a keychain identity signs through
Security, so `codesign -s "Apple Development"` produces a signature Apple's own
`codesign --verify` accepts as valid and as satisfying its designated
requirement), `xcrun`,
`xcodebuild`, `xcode-select`, `pkgbuild`, `productbuild`, `simctl`,
`notarytool`, `devicectl`, `xctrace` (stub).

Also `make_obj_file_with_linker_options()`, reimplemented from scratch because
Apple ships neither the source nor the library (`libcctoolshelper`) that
`libtool` needs.

## Building

Requires Xcode (or the Command Line Tools) and `bmake`.

```sh
git clone --recurse-submodules https://github.com/xnuports/xcode-tools.git
cd xcode-tools
git submodule update --init --recursive
bmake
bmake check        # verify every inventory entry produced a binary
```

Everything lands in `build/release/`. There is no `install` target — the
release tree *is* the product, and the tools locate their own Developer
directory from the running binary, so a built or moved tree works with no
configuration:

```sh
build/release/usr/bin/xcrun --find ld
build/release/usr/bin/xcodebuild -showsdks
```

Useful targets:

```sh
bmake                   # build, then emit the bundle metadata
bmake check             # every inventory entry produced a binary
bmake list-progs        # the program inventory
bmake list-ports        # the port inventory
bmake clean             # remove build/, keeping the ports work directories
bmake clean-ports       # remove the ports work directories
bmake distclean         # remove build/ entirely
```

### Tiers

```sh
bmake MK_TOOLCHAIN=no   # skip the binutils tier (cctools, ld64)
bmake MK_PORTS=yes      # also build components with their own build system
```

`MK_PORTS` is off by default because it includes LLVM: most of an hour on ten
cores, and 2.7 GB of build directory. It is what supplies `clang`, the `llvm-*`
tools, and `libtapi` — and therefore `ld`, which links against it. Note that
`clean` deliberately spares `build/ports`, so an ordinary rebuild does not
throw that away.

## Build system

`bmake`, with two engines driven by flat inventories — the same architecture as
the sibling [apple-core](https://github.com/xnuports/apple-core) project:

| | |
|---|---|
| `mk/tool.mk` | compiles a program from sources; driven by `mk/progs.mk` |
| `mk/port.mk` | drives a component's own build system (autoconf, CMake); driven by `mk/ports.mk` |
| `mk/tool.d/`, `mk/port.d/` | per-entry flags |
| `mk/with-*.mk` | reusable link bundles |
| `mk/bundle.mk` | emits the `.xctoolchain` and `.sdk` metadata |
| `mk/patches/` | patches applied to a port's private copy |

Adding a tool is one line in `mk/progs.mk`, plus a `mk/tool.d/<tool>.mk` only if
it needs flags — sources are discovered automatically.

**Submodules are never written to.** Every Makefile lives outside them and
reaches in read-only; ports that cannot build out of tree get a private copy.
`bmake check` exists because per-tool failures are ignored on purpose, so a
broken tool would otherwise vanish from the release tree unnoticed.

Two clean builds of the default set produce byte-identical binaries.

## Layout

| Path | |
|---|---|
| `src/xcode/` | our reimplementations (BSD-3-Clause) |
| `src/xcode/common/` | shared helpers: plist parsers, SDK discovery, self-location |
| `src/` | source submodules — `distribution-Developer_Tools` (cctools, ld64, tapi, …), llvm-project, swift, cpython, git, bmake, bsdmake |
| `lib/` | library submodules — corecrypto, dyld, libplatform, libdispatch, `apple_internal_sdk` |
| `include/` | headers vendored where the SDK ships none |
| `mk/` | the build system |
| `configs/`, `scripts/` | inputs to bundle emission |
| `docs/CLAUDE.md` | full development notes, roadmap and rationale |

## What is missing

- **SDK contents** — headers, libraries, frameworks. The bundle exists; the
  contents do not.
- **Swift** — the submodule is there, not yet built.
- **Python, Git** — sources carried (CPython 3.14.6, git 2.50.1, the version Apple's Git-155 wraps), not yet ported to `mk/port.mk`.
- **The `xc*` family** — `xccov`, `xcresulttool`, `xcstringstool`,
  `xcsigningtool`, `xctest`, `xcdevice`, `xcdiagnose`.
- **Certificate signing has two gaps left.** Signing with a keychain identity
  is complete and verifies. Signing from a `.p12` still reports no authority,
  because a self-signed certificate cannot satisfy the `anchor apple generic`
  term our designated requirement always emits. The Apple hash-agility version 2
  attribute is also omitted: its dictionary is keyed by an algorithm identifier
  with no documented mapping, and a wrong key yields an attribute that makes the
  signature unreadable. Version 1, which carries the cdhashes property list, is
  emitted.
- **`codesign` does not implement `-i`/`--identifier`,** and cannot sign
  universal binaries — signing a fat file yields "invalid or unsupported format
  for signature". Both predate the signing work above.
- **`vmmap` is unverified** — it builds, but examining a process needs
  `task_for_pid`, which macOS grants only to root or an entitled binary.
  Apple's copy is codesigned for it; ours is not, so it reports a privilege
  error rather than a memory map unless run under `sudo`.
- **Apple-proprietary tools** — `actool`, `ibtool`, `momc`, `coremlc` and the
  rest have no published source and need reimplementation.

`docs/CLAUDE.md` has the full picture, including what each component needed and
why several of them were harder than they looked.

## Sources

Submodules track upstream rather than forks: llvm-project and swift from
swiftlang, the Apple components from apple-oss-distributions (corecrypto from
`apple/corecrypto`), CPython from python/cpython — each pinned to a release tag
or branch. Currently clang 21.1.6 (`swift-6.3.3-RELEASE`), ld64 957.1, cctools
1035.1.102, tapi 1600.0.11.8, bmake mk-20260808, bsdmake bsdmake-24.

A few submodules stay on xnuports forks — `apple_internal_sdk`, `ld-internals`,
`PlistBuddy`, `pngcrush`, `python-apple-support` — because they carry local
changes or have no upstream to track.

## License

Our code is BSD-3-Clause (`LICENSE.BSD-3`). Submodules keep their own licences:
Apache-2.0 with LLVM exception, APSL, GPL, BSD, MIT, PSF and others.
