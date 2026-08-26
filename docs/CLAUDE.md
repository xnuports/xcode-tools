# CLAUDE.md — xcode-tools Project Roadmap

> Instructions for Claude (or any AI agent) working on the `xcode-tools` project.
> Read this entire file before starting any work. It consolidates the project's
> goals, current state, structure, and a phased roadmap.

---

## 1. Project Vision

Produce a **fully open-source SDK bundle** that is **function-to-function identical**
to Apple's proprietary Xcode `Developer/` directory — including the LLVM toolchain
(with Swift and Objective-C support), all command-line developer tools, SDKs,
platforms, and the `usr/bin` utilities shipped with Xcode.

One should be able to swap Apple's proprietary tools out for ours without any
issue. Every binary should behave identically to its Apple counterpart when
invoked from the same paths with the same arguments.

---

## 2. Core Principles

1. **Function-to-function parity:** Every tool we ship must be interchangeable
   with Apple's. Output, exit codes, and side-effects must match.
2. **Open source first:** Prefer upstream open-source code. Where Apple has not
   released source, write our own reimplementation (marked as our own code).
3. **bmake compatibility:** All build system work must compile under `bmake`
   (BSD make). No GNU make extensions like `$(shell ...)`, `$(CURDIR)`,
   or GNU-style pattern rules with path prefixes. Use `.for` loops, `!=`
   for command substitution, and `${.CURDIR}`.
4. **Build hygiene:** All build artifacts go to `build/` (`build/release/`
   for the staged Developer tree, `build/obj/<dir>/` for objects). The source
   tree, submodules included, must remain pristine after a full build.
5. **Submodule discipline:** Each external dependency is a git submodule.
   Nested submodules must be populated with `git submodule update --init --recursive`.

---

## 3. Current Repository Structure

```
xcode-tools/
├── Makefile                        # Top-level bmake build system
├── .gitmodules                     # 11 submodule definitions
├── .gitignore
├── LICENSE.BSD-3
├── README.md
├── mk/                             # THE BUILD SYSTEM (see section 8)
│   ├── xcodetools.sys.mk           # global flags, tier gating
│   ├── progs.mk                    # the program inventory
│   ├── tool.mk                     # the per-program engine
│   ├── tool.d/<program>.mk         # optional per-tool flags
│   ├── bundle.mk                   # .xctoolchain / .sdk emission
│   └── with-*.mk                   # reusable link bundles
├── lib/
│   └── Makefile                    # static libs from submodule sources
├── docs/
│   ├── DOCUMENTATION.md            # Comprehensive audit vs. Xcode Developer
│   └── CLAUDE.md                   # This file
├── configs/                        # SDK/toolchain INI configuration (stage 3 input)
├── scripts/                        # Toolchain shim scripts (stage 3 input)
├── xctoolchain/                    # Submodule: Xcode toolchain configurations
├── include/
│   ├── mach-o/                     # OURS: headers Apple references but never shipped
│   └── ld-internals/               # Submodule: ld64 private headers
├── src/
│   ├── Makefile                    # .for loop over mk/progs.mk
│   ├── PlistBuddy/                 # Submodule: open-source PlistBuddy
│   ├── python-apple-support/       # Submodule: Python build system for Apple platforms
│   ├── cpython/                    # Submodule: CPython 3.14.7 source
│   ├── git/                        # Submodule: Git v2.55.0 source
│   ├── dist-dev-tools/             # Submodule: Apple open-source dev tools (nested)
│   │   ├── CoreOSMakefiles/        # Build system makefiles
│   │   ├── Git/                    # Apple's Git fork (Git-155)
│   │   ├── bison/                  # GNU Bison
│   │   ├── bootstrap_cmds/         # Bootstrap commands
│   │   ├── cctools/                # ar, nm, lipo, strip, otool, vtool, install_name_tool, etc.
│   │   ├── developer_cmds/         # asa, ctags, indent, lorder, rpcgen, unifdef
│   │   ├── flex/                   # Fast Lexical Analyzer
│   │   ├── gm4/                    # GNU M4
│   │   ├── gnumake/                # GNU Make
│   │   ├── gperf/                  # Perfect hash function generator
│   │   ├── headerdoc/              # headerdoc2html, hdxml2manxml, gatherHeaderDoc
│   │   ├── ld64/                   # Apple linker (ld, ld-classic)
│   │   ├── libgit2/                # Git library
│   │   ├── pb_makefiles/           # Project Builder makefiles
│   │   └── tapi/                   # Text-based API tool
│   ├── objc4/                      # Submodule: Objective-C runtime
│   ├── pngcrush/                   # Submodule: pngcrush v1.8.1
│   ├── llvm-project/               # Submodule: LLVM/Clang/LLDB/MLIR/flang/etc.
│   ├── swift/                      # Submodule: Swift compiler
│   ├── cctools-helpers/            # OURS: reimplemented libcctoolshelper piece
│   ├── common/                     # OURS: shared helpers (devpath.c)
│   └── xcode/                      # OUR reimplemented Xcode tools (10 tools)
│       ├── codesign/               # ad-hoc signing & verification (7 files)
│       ├── devicectl/              # device management (2 files)
│       ├── notarytool/             # notarization client (6 files)
│       ├── pkgbuild/               # package building (6 files)
│       ├── productbuild/           # product building (7 files)
│       ├── simctl/                 # simulator control (5 files)
│       ├── xcode-select/           # developer dir selection (1 file)
│       ├── xcodebuild/             # build orchestration (6 files)
│       ├── xcrun/                  # tool locator & executor (3 files)
│       └── xctrace/                # trace recording (4 files, stub)
└── build/                          # Generated output
    ├── obj/<dir>/                  # object files
    ├── gen/<tool>/                 # build-time generated sources
    ├── lib/                        # static libraries
    └── release/                    # staged, drop-in Developer/ tree
```

---

## 4. Tools We Have Implemented (10)

Our own BSD-licensed reimplementations in `src/xcode/`:

| # | Tool | Lines of Code | Status | Primary Gap |
|---|------|--------------|--------|-------------|
| 1 | **codesign** | ~2,500 lines | ✅ Ad-hoc signing works | No CMS/cert-based signing |
| 2 | **devicectl** | ~650 lines | ✅ Device listing/pairing | No remote mgmt, app install, I/O |
| 3 | **notarytool** | ~1,200 lines | ✅ Submission via API | No history/log/cancel commands |
| 4 | **pkgbuild** | ~900 lines | ✅ Package creation | Missing some edge cases |
| 5 | **productbuild** | ~1,100 lines | ✅ Product archive | Missing some distribution options |
| 6 | **simctl** | ~700 lines | ✅ Basic simulator ops | Missing io/push/location/env |
| 7 | **xcode-select** | ~250 lines | ✅ Path selection | Missing `--install` |
| 8 | **xcodebuild** | ~1,200 lines | ✅ Orchestration only | No actual compilation (delegates) |
| 9 | **xcrun** | ~400 lines | ✅ Tool location | Missing SDK resolution edge cases |
| 10 | **xctrace** | ~400 lines | ✅ Stub | No actual tracing backend |

**Key: `xcodebuild` currently delegates compilation to the toolchain (via `xcrun`).
It does not perform per-file compilation itself. Full xcodebuild build phase
execution requires the LLVM toolchain to be built and installed first.**

---

## 5. Submodules with Available Source

These submodules provide source code for tools previously listed as "no source":

| Submodule | Path | Version | Tools Covered |
|-----------|------|---------|---------------|
| `llvm-project` | `src/llvm-project/` | swift-6.4.x | clang, swift-frontend, llvm-*, lld, lldb, dsymutil, dwarfdump, clang-format, clangd |
| `swift` | `src/swift/` | heads/main | swiftc, swift-frontend, swift-driver, SPM, swift-format, sourcekitd |
| `objc4` | `src/objc4/` | heads/main | ObjC runtime (libobjc.A.dylib) |
| `dist-dev-tools` | `src/dist-dev-tools/` | 26.0.1 | See nested submodules below |
| `git` | `src/git/` | v2.55.0 | git, git-receive-pack, git-shell, git-upload-pack |
| `cpython` | `src/cpython/` | v3.14.7 | python3, pip3, pydoc3, 2to3 |
| `python-apple-support` | `src/python-apple-support/` | heads/main | **Reference only** — a meta-build system for Python XCFrameworks. We build Python ourselves, so this is kept for reference rather than used. |
| `PlistBuddy` | `src/PlistBuddy/` | heads/main | PlistBuddy |
| `pngcrush` | `src/pngcrush/` | v1.8.1 | pngcrush |
| `xctoolchain` | `xctoolchain/` | heads/master | **Reference only** — generic `.xcconfig` build settings, not a source of the `.xctoolchain` bundle format |
| `ld-internals` | `include/ld-internals/` | heads/main | ld64 private headers |

### dist-dev-tools Nested Submodules

| Sub-submodule | Path | Version | Tools |
|--------------|------|---------|-------|
| `cctools` | `src/dist-dev-tools/cctools/` | 1030.6.3 | ar, nm, lipo, strip, otool, vtool, install_name_tool, bitcode_strip, codesign_allocate, ctf_insert, libtool, segedit, etc. |
| `ld64` | `src/dist-dev-tools/ld64/` | 956.6 | ld, ld-classic |
| `developer_cmds` | `src/dist-dev-tools/developer_cmds/` | 87 | asa, ctags, indent, lorder, rpcgen, unifdef |
| `headerdoc` | `src/dist-dev-tools/headerdoc/` | 8.9.32 | headerdoc2html, hdxml2manxml |
| `bison` | `src/dist-dev-tools/bison/` | 16 | bison |
| `flex` | `src/dist-dev-tools/flex/` | 35 | flex, flex++ |
| `gnumake` | `src/dist-dev-tools/gnumake/` | 136 | make, gnumake |
| `gperf` | `src/dist-dev-tools/gperf/` | 15 | gperf |
| `gm4` | `src/dist-dev-tools/gm4/` | 19 | gm4 |
| `tapi` | `src/dist-dev-tools/tapi/` | 1600.0.11.8 | tapi |
| `pb_makefiles` | `src/dist-dev-tools/pb_makefiles/` | 1009 | Build system makefiles |
| `bootstrap_cmds` | `src/dist-dev-tools/bootstrap_cmds/` | 138 | Bootstrap commands |
| `CoreOSMakefiles` | `src/dist-dev-tools/CoreOSMakefiles/` | 79 | Build system infrastructure |
| `Git` | `src/dist-dev-tools/Git/` | 155 | Apple's Git fork |
| `libgit2` | `src/dist-dev-tools/libgit2/` | 30 | Git library |

---

## 6. Tools Still Missing (No Source)

### 6.1 Apple-Specific / Proprietary (binary-only in Xcode)

| Tool | Category | Status |
|------|----------|--------|
| actool | Asset Catalog | ❌ No source (Apple proprietary) |
| ibtool, ibtoold | Interface Builder | ❌ No source (Apple proprietary) |
| coremlc | Core ML | ❌ No source (Apple proprietary) |
| momc | Core Data | ❌ No source (Apple proprietary) |
| ictool | Asset inspection | ❌ No source (Apple proprietary) |
| instrumentbuilder | Instruments | ❌ No source (Apple proprietary) |
| realitytool | AR | ❌ No source (Apple proprietary) |
| referenceobjectc | AR | ❌ No source (Apple proprietary) |
| scntool | SceneKit | ❌ No source (Apple proprietary) |
| compileSceneKitShaders | SceneKit | ❌ No source (Apple proprietary) |
| copySceneKitAssets | SceneKit | ❌ No source (Apple proprietary) |
| mapc | Maps | ❌ No source (Apple proprietary) |
| altool | App Store | ❌ No source (Apple proprietary) |
| iTMSTransporter | App Store | ❌ No source (Apple proprietary) |
| ipatool, ipatool2 | App Store | ❌ No source (Apple proprietary) |
| cktool | Code Signing | ❌ No source (Apple proprietary) |
| xcsigningtool | Code Signing | ❌ No source (Apple proprietary) |
| stapler | Code Signing | ❌ No source (Apple proprietary) |
| embeddedBinaryValidationUtility | Code Signing | ❌ No source (Apple proprietary) |
| xccov | Testing | ❌ No source (Apple proprietary) |
| xcresulttool | Testing | ❌ No source (Apple proprietary) |
| xcstringstool | Localization | ❌ No source (Apple proprietary) |
| xctest | Testing | ❌ No source (Apple proprietary) |
| xed | IDE | ❌ GUI app, not a CLI tool |
| xcdebug | Debug | ❌ No source (Apple proprietary) |
| xcindex-test | Debug | ❌ No source (Apple proprietary) |
| xcdevice | Device | ❌ No source (Apple proprietary) |
| xcdiagnose | Debug | ❌ No source (Apple proprietary) |
| atos | Debug | ❌ No source (Apple proprietary) |
| vmmap | Debug | ❌ No source (Apple proprietary) |
| symbols | Debug | ❌ No source (Apple proprietary) |
| leaks | Debug | ❌ No source (Apple proprietary) |
| malloc_history | Debug | ❌ No source (Apple proprietary) |
| heap | Debug | ❌ No source (Apple proprietary) |
| xctrace record | Tracing | ❌ Backend requires DTR (private) |
| sdef, sdp | Scripting | ❌ No source (Apple proprietary) |
| agvtool | Version | ❌ No source (Apple proprietary) |
| crashlog | Debug | ❌ No source (Apple proprietary) |
| CreateIPA | App Store | ❌ No source (Apple proprietary) |
| iphoneos-optimize | Asset | ❌ No source (Apple proprietary) |
| placeholderutil | App Store | ❌ No source (Apple proprietary) |
| xml2man | Docs | ✅ Source: `src/dist-dev-tools/headerdoc/xmlman/` — built |
| agent, ba-package, ba-serve | Build Assistant | ❌ No source (Apple internal) |
| backgroundassets-debug | Debug | ❌ No source (Apple internal) |
| compositeMD5 | Archive | ❌ No source (Apple internal) |
| convertRichTextToAscii | Conversion | ❌ No source (Apple internal) |
| filtercalltree | Debug | ❌ No source (Apple internal) |
| resolveLinks | File | ✅ Source: `src/dist-dev-tools/headerdoc/xmlman/` — built |
| swinfo | File | ❌ No source (Apple internal) |
| stringdups | File | ❌ No source (Apple internal) |
| extractLocStrings | Localization | ❌ No source (Apple proprietary) |
| gatherheaderdoc | Docs | ✅ Available via dist-dev-tools/headerdoc (as `gatherHeaderDoc.pl`) |
| unifdef | Dev command | ✅ Source: `src/dist-dev-tools/developer_cmds/unifdef/` |
| c89, c99 | Compatibility | ✅ Covered by clang (aliased) |
| metal, metal-package-builder | Graphics | ❌ No source (Apple proprietary) |
| mig | IPC | ❌ No source (not in open-source releases) |
| unwinddump | Debug | ❌ No source (Apple proprietary) |

### 6.2 Resource Fork Tools (binary-only in `Developer/Tools/`)

| Tool | Description | Status |
|------|-------------|--------|
| DeRez | Resource de-compiler | ❌ Binary-only, no source |
| Rez | Resource compiler | ❌ Binary-only, no source |
| ResMerger | Resource merger | ❌ Binary-only, no source |
| GetFileInfo | File metadata query | ❌ Binary-only, no source |
| SetFile | File attribute setter | ❌ Binary-only, no source |
| SplitForks | Fork splitter | ❌ Binary-only, no source |

### 6.3 Xcode Toolchain Tools (no source)

| Tool | Category | Status |
|------|----------|--------|
| appintentsmetadataprocessor | App Intents | ❌ Apple proprietary |
| appintentsnltrainingprocessor | App Intents | ❌ Apple proprietary |
| appshortcutstringsprocessor | App Intents | ❌ Apple proprietary |
| cache-build-session | Build | ❌ Apple proprietary |
| clang-cache | Build | ❌ Apple proprietary |
| clang-cas-test | Build | ❌ Apple proprietary |
| createml | ML | ❌ Apple proprietary |
| exutil | Executor | ❌ Apple proprietary |
| iig | IPC | ❌ Apple proprietary |
| llvm-cas | Build | ❌ Apple proprietary |
| modules-verifier | Build | ❌ Apple proprietary |
| snippet-extract | Build | ❌ Apple proprietary |
| swift-experimental-sdk | Swift | ❌ Apple proprietary |
| swift-package-collection | SPM | ❌ Not in open-source Swift |
| swift-package-registry | SPM | ❌ Not in open-source Swift |
| swift-plugin-server | Swift | ✅ Source in `src/swift/` |
| swift-stdlib-tool | Swift | ✅ Source in `src/swift/` |
| swift-static | Swift | ❌ Apple proprietary |
| swift-symbolgraph-extract | Swift | ✅ Source in `src/swift/` |
| swift-synthesize-interface | Swift | ✅ Source in `src/swift/` |

---

## 7. SDK & Platform Infrastructure

### 7.1 Platforms (10, Apple proprietary — no source)

- AppleTVOS.platform
- AppleTVSimulator.platform
- DriverKit.platform
- iPhoneOS.platform / iPhoneSimulator.platform
- MacOSX.platform
- WatchOS.platform / WatchSimulator.platform
- XROS.platform / XRSimulator.platform

Each contains: Info.plist, SDKs/, usr/ (headers, libs), Tools/, Developer/.

### 7.2 SDKs (10 — no source)

- AppleTVOS.sdk, AppleTVSimulator.sdk
- DriverKit.sdk
- iPhoneOS.sdk, iPhoneSimulator.sdk
- MacOSX.sdk (multiple versions)
- WatchOS.sdk, WatchSimulator.sdk
- XROS.sdk, XRSimulator.sdk

### 7.3 What We Provide as Infrastructure

- `configs/` — INI-format SDK/toolchain configuration
- `scripts/` — toolchain shim scripts (cc.sh, clang.sh, xcrun-tool.sh)
- `xctoolchain/Configurations/` — .xcconfig files for build settings

**Apple uses:** SDKSettings.plist (property list), .xcconfig files, .platform bundles

---

## 8. Build System

### 8.1 Our Build System

**Build tool:** `bmake` (BSD make)

The architecture is ported from the sibling `apple-core` / `darwintools`
project: one generic engine driven by a flat inventory, rather than a
Makefile per tool.

```
Makefile          top level: dirs, lib, progs
  → lib/Makefile      static libraries from submodule sources
  → src/Makefile      .for loop over mk/progs.mk
      → mk/tool.mk    invoked once per program
```

| File | Role |
|------|------|
| `mk/xcodetools.sys.mk` | global flags, tier gating, `DISABLED_PROGS` filter |
| `mk/progs.mk` | the inventory: `PROGS+= <src-dir> <program> <install-suffix>` |
| `mk/tool.mk` | the per-program engine (never invoked by hand) |
| `mk/port.mk` | the per-port engine, for foreign build systems |
| `mk/ports.mk` | the port inventory: `PORTS+= <src-dir> <port> <suffix>` |
| `mk/tool.d/<program>.mk` | optional per-tool flags, `sinclude`d |
| `mk/with-*.mk` | reusable link bundles (e.g. `with-openssl.mk`) |

`mk/tool.mk` discovers `.c/.cc/.cpp/.y/.l` sources automatically, runs
yacc/lex codegen, and links with `${CXX}` when any source is C++. Per-tool
overrides: `T_SRCS`, `T_CFLAGS`, `T_LDADD`, `T_LINKS`, `T_SCRIPT`, `T_NOBUILD`.

**Hard rule:** submodules are never written into. Every Makefile lives outside
them and reaches in read-only via `.PATH`. Objects go to `build/obj/`, so a
full build leaves every submodule pristine. Submodule changes must go through
the individual upstream repositories.

**Key design decisions for bmake compatibility:**
- `${.CURDIR}` instead of `$(CURDIR)` for directory paths
- `!=` operator instead of `$(shell ...)` for command substitution
- `.for` loops to generate explicit compile rules (bmake doesn't support
  path-prefixed `%.o: %.c` pattern rules)
- **Plain `=`, not `?=`, for `CC`/`CFLAGS`/`CXXFLAGS`.** bmake predefines all
  three in its own `sys.mk` (`CC` is `cc ${PIPE}`, `CFLAGS` is `-O2`), so `?=`
  is silently a no-op and your flags vanish. Command-line assignments still win.
- `-Wl,-reproducible` on every link. With `-g`, ld records each object's mtime
  in the debug map (`N_OSO` stab) and folds it into `LC_UUID`, so two clean
  builds of identical sources otherwise differ byte-for-byte.
- `-Werror` applies only to our own sources under `src/xcode/`; imported
  Apple/GNU sources predate most modern diagnostics and are exempt.

### 8.2 Output layout

`build/release/` is a drop-in replacement for Xcode's `Developer/` directory,
and is the product — there is no `install` target and no `PREFIX`.

```
build/obj/<dir>       per-tool object files
build/gen/<tool>      build-time generated sources
build/lib             static libraries
build/release/
  usr/{bin,lib,libexec,share}
  Toolchains/XcodeDefault.xctoolchain/usr/bin
  Platforms/<P>.platform/Developer/SDKs/<S>.sdk
  Tools/
```

### 8.3 Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TOP` | `${.CURDIR}` | Repository root, passed down to every sub-make |
| `CC` / `CXX` | `cc -pipe` / `c++` | Compilers |
| `CFLAGS` | `-O2 -g -Wall -Wno-unused-parameter` | Global C flags |
| `XCTOOLCHAIN` | `Toolchains/XcodeDefault.xctoolchain` | Toolchain bundle path under `build/release/` |
| `MK_TOOLCHAIN` | `yes` | Build the binutils tier (cctools, ld64) |

## 9. Phased Roadmap

Five stages, in dependency order. Stage 1 is done; stage 2 is the current work.

### Stage 1 — Core build system ✅

Port `apple-core`'s `mk/` architecture (section 8) and migrate our ten tools
onto it. Delivered: one engine, a flat inventory, `build/release/` as a
drop-in `Developer/` tree, byte-reproducible links, and ten fewer Makefiles.

### Stage 2 — Everything buildable today (current)

Wire up the submodule sources that `mk/tool.mk` can compile directly. None of
these need a foreign build system.

#### 2a. cctools ✅

15 programs build and install to `${XCTOOLCHAIN}/usr/bin`:

    ar   bitcode_strip   codesign_allocate   ctf_insert   install_name_tool
    libtool (+ranlib)   lipo   nm-classic   nmedit   otool-classic
    segedit   size-classic (+size)   strings   strip   vtool

`libstuff` (46 sources) and `libmacho` build as static libraries in
`lib/Makefile`; consumers pull them in via `mk/with-libstuff.mk` and
`mk/with-libmacho.mk`, which imply `mk/with-cctools.mk` for the shared
include paths and defines translated out of Apple's xcconfigs.

Four things worth knowing, all of them non-obvious:

1. **The `-classic` names.** Modern Xcode has retired the cctools builds of
   `nm`, `otool` and `size` from their plain names. In a stock toolchain `nm`
   and `otool` are symlinks to `llvm-nm` / `llvm-otool`, and `size` is a
   symlink to `size-classic`; the cctools builds ship as `nm-classic`,
   `otool-classic`, `size-classic`. We install under those names and leave
   `nm` and `otool` for llvm-project in stage 5. `nmedit` is `strip.c`
   recompiled with `-DNMEDIT` — cctools has no `nmedit.c`.

2. **`libtool` needed a reimplemented Apple helper.** `libtool.c` calls
   `make_obj_file_with_linker_options()`, declared in
   `<mach-o/cctools_helpers.h>` and defined in `libcctoolshelper`. Neither the
   header nor the library appears in the open-source drop *or* in a shipped
   Xcode — they are build-time-only Apple internals, statically linked into
   Apple's binaries.

   `src/cctools-helpers/` is our BSD-licensed reimplementation, with the
   declaration in `include/mach-o/cctools_helpers.h` so `libtool.c` compiles
   unmodified. It writes an `MH_OBJECT` holding one `LC_LINKER_OPTION` per
   requested library and framework, which libtool adds to the archive as
   `__ALWAYS_LOAD.o`; the linker force-loads that member and resolves the
   references. The encoding ld64 accepts is exactly two shapes
   (`src/ld/Resolver.cpp`): one string `-lNAME`, or the pair `-framework`,
   `NAME`. Anything else draws "unknown linker option from object file
   ignored".

   Two things that cost time and are worth knowing:
   - An `MH_OBJECT` must carry a segment load command even with no sections.
     Without one ld64 rejects it with "missing LC_SEGMENT file ... for
     architecture", so the generated object emits a single empty segment.
   - A framework resolved this way does not appear in `otool -L` unless a
     symbol from it is actually used — ld does not record an unused dylib. Test
     it by referencing a symbol without passing `-framework`, not by looking
     for the load command.

   `ranlib` is the same binary dispatched on `argv[0]`, and comes from
   `T_LINKS` in `mk/tool.d/libtool.mk`.

   Note that Apple's *shipped* `libtool` and `ranlib` are not this program at
   all — their usage text is entirely different, and the source is in neither
   cctools nor ld64. Like `nm` and `otool`, Apple has replaced them with a
   newer, unpublished implementation. Ours is the cctools one, which builds
   working archives.

   A defect in the published `libtool.c` is worth recording: `add_member()`
   skips the `stat()` that fills a member's `ar_hdr` when the member is named
   `__ALWAYS_LOAD.o` (line 1974), and nothing else fills it, so that member
   lands in the archive with an uninitialised mode, uid/gid and date. It is
   metadata only — the linker reads members from the archive image and links
   fine — but an extracted `__ALWAYS_LOAD.o` is unreadable until chmod'd. We
   do not patch it, since submodules are never edited.

   `libcctoolshelper` is also why `strip` is built without `-DTRIE_SUPPORT`,
   which `strip.xcconfig` sets for macOS/Xcode builds: the guarded code
   includes the same missing header. `strip` is otherwise complete.

3. **`CODEDIRECTORY_SUPPORT` is deliberately off.** Apple enables it for macOS
   and Xcode builds, but it links `libcodedirectory.dylib`, a closed Apple
   binary. Depending on it would defeat the purpose of the project. The cost
   is that tools which rewrite a Mach-O do not regenerate its ad-hoc code
   signature — see the parity note in section 11.

4. **Two gaps in the published sources** had to be filled from our own
   submodules, both recorded in `mk/cctools-compat.h`:
   - `libstuff/lto.c` includes `<llvm-c/lto.h>`, which the drop does not
     bundle (its `include/llvm-c/` holds only `Disassembler.h`). Taken from
     `src/llvm-project/llvm/include`.
   - `libstuff/reloc.c` switches on `CPU_TYPE_RISCV32`, which is defined
     nowhere: cctools' own `include/mach/machine-cctools.h` carries the
     "RISC-V subtypes" section comment with the definitions beneath it
     stripped, and the public SDK stops at `CPU_TYPE_POWERPC64`. Recovered
     from LLVM's `BinaryFormat/MachO.h` (`CPU_TYPE_RISCV = 24`). Apple treats
     riscv32 as absent from this toolchain anyway: ld64's `create_configure`
     emits `SUPPORT_ARCH_riscv32 0` whenever the install dir matches
     `XcodeDefault`.

#### 2b. developer_cmds, headerdoc, pngcrush, PlistBuddy ✅

    ${XCTOOLCHAIN}/usr/bin   asa  ctags  indent  lorder  rpcgen  unifdef
    usr/bin                  headerdoc2html  hdxml2manxml  gatherheaderdoc
                             xml2man  resolveLinks  pngcrush
    usr/libexec              PlistBuddy

Notes:

- **developer_cmds go in the toolchain, not `Developer/usr/bin`.** Xcode ships
  all six in `XcodeDefault.xctoolchain/usr/bin`. Do not confuse them with the
  system copies in `/usr/bin`, which belong to apple-core. `lorder` is a shell
  script, installed via `T_SCRIPT`.

- **`xml2man` and `resolveLinks` are available**, contrary to section 6, which
  lists `xml2man` as Apple-proprietary and `resolveLinks` as Apple-internal.
  Both are shipped by Xcode in `Developer/usr/bin` and both build from
  headerdoc's `xmlman/`. That directory holds three programs plus a
  `strcompat.c`, so each entry pins `T_SRCS`. `strcompat.c` is excluded
  deliberately: it defines `strlcpy`/`strlcat`, and headerdoc's own Makefile
  compiles it only on Linux (`COMPATIBILITY_BITS` is empty on Darwin, where
  libc has them). All three link `-lxml2`.

- **pngcrush needed two fixes.** libpng's `pngpriv.h` reaches for `<fp.h>`, the
  Classic Mac OS math header, because `TARGET_OS_MAC` is defined on modern
  macOS too; its guard skips that include once `<math.h>` has been seen, so
  `-include math.h` is the fix the header itself anticipates. And `pngrutil.c`
  calls `png_init_filter_functions_neon` while shipping no `arm/` directory to
  define it, so `-DPNG_ARM_NEON_OPT=0` turns that path off.

  The bundled zlib is incomplete — `gzguts.h` and every `gz*.c` are missing, so
  `zutil.c` will not compile, and zlib's own `-DZ_SOLO` switch for a gz-less
  build then drops `zcalloc`/`zcfree`, which deflate needs. We link the system
  zlib instead, a configuration pngcrush's own Makefile supports, which also
  resolves the bundled-vs-system concern in `docs/DOCUMENTATION.md` section 4.
  `filter_sse2_intrinsics.c` is excluded as x86-only.

  Our pngcrush is *newer* than Apple's: 1.8.1 with libpng 1.6.21 against their
  1.6.4 with libpng 1.2.7. Output is therefore not byte-identical — ours
  compresses slightly better — but the decoded images are: same IHDR, and
  byte-identical raw scanlines. Compare pixels, not file bytes.

- **PlistBuddy is not part of Xcode at all.** Stock macOS ships it at
  `/usr/libexec/PlistBuddy`; there is no copy in the Developer directory. It is
  built here because the project carries it as a submodule, installed to
  `usr/libexec` as the closest match. It links `-framework CoreFoundation`.

#### 2c. ld64 ✅

`ld` builds and links. It needs `libtapi`, which the llvm port stages, so it
only appears with `MK_PORTS=yes`; `mk/progs.mk` gates the entry on that.

End to end, with our own toolchain throughout:

```
$ clang -isysroot $SDK -arch arm64e -c e2e.c -o e2e.o     # our clang
$ ld -o e2e e2e.o -lSystem -syslibroot $SDK -arch arm64e \
     -platform_version macos 26.0 26.0                    # our ld64
$ ./e2e
end-to-end: our clang, our ld, our libtapi
```

`ld -v` reports in Apple's format:
`@(#)PROGRAM:ld  PROJECT:ld64-956.6`.

**Known gap: `-arch arm64` does not link against the current SDK; `arm64e`
does.** The SDK's `libSystem.tbd` declares its targets and reexports as
`[x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst]` — there
is no `arm64-macos` — so an `-arch arm64` link finds the stub but resolves none
of its reexported symbols. Apple's `ld` *and* `ld-classic` both link `arm64`
against the same SDK, so they treat an arm64e slice as satisfying an arm64
request somewhere we do not. Neither ld64 956.6's `textstub_dylib_file.cpp` nor
tapi 2.0.0's `LinkerInterfaceFile.cpp` contains any arm64/arm64e fallback, so
this is in Apple's newer builds (their ld-classic is 957.1) rather than
something misconfigured here. Worth chasing before ld64 can be called a
drop-in.

What it took to get here is in `mk/with-ld64.mk` and `mk/tool.d/ld.mk`:
C++20, `-lxar`, the private-header paths, and four generated files standing in
for the xcodeproj's script phases — `configure.h` from `src/create_configure`,
`compile_stubs.h` as a C string, `tapi/Version.inc`, and `version.c` supplying
`ld_classicVersionString`, which `Options.cpp` declares `extern` but no source
defines.

Three traps worth not rediscovering:

1. **cctools ships its own stripped `mach-o/dyld_priv.h`**, without
   `dyld_unwind_sections`. dyld's copy has to win the header search — but
   putting all of `lib/dyld/include` first is wrong, because it also carries
   `dlfcn.h` and `mach-o/dyld.h`, which shadow the system and cctools headers
   and break four otherwise-fine files. A generated shim directory exposing
   only that one header avoids both.
2. **Several Apple headers annotate with `bridgeos()`**, which the public SDK's
   availability macros reject. The shims neuter `__API_AVAILABLE` and friends
   around the include and restore them with `#pragma push_macro`/`pop_macro`;
   clobbering them for the rest of the file breaks later system headers.
3. **`ld/passes/stubs/` is a subdirectory of `passes/`** and is easy to miss
   when listing sources — the link fails on `ld::passes::stubs::doPass`.

### Stage 3 — `.xctoolchain` and `.sdk` bundle emission ✅

`mk/bundle.mk` emits the metadata that makes `build/release/` a Developer
directory rather than a loose bin tree, via the `bundles` target:

```
Toolchains/XcodeDefault.xctoolchain/ToolchainInfo.plist   Identifier
Platforms/MacOSX.platform/Info.plist                      platform bundle
Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/SDKSettings.plist
usr/share/xcrun.ini                                       xcrun's defaults
```

Version, deployment target and default arch are taken from the host SDK so the
emitted bundle is coherent with what the tools would build against; override
`XT_SDK_VERSION` and friends to pin them. The SDK is a skeleton — settings but
no headers or libraries yet — emitted anyway because it defines the layout
every later stage installs into.

The toolchain identifier is deliberately Apple's
(`com.apple.dt.toolchain.XcodeDefault`): it is the name build systems look up
to find the default toolchain, so a replacement has to answer to it, exactly as
our tools have to answer to Apple's argv.

**The tools are now self-locating, and no path is compiled in.**
`src/xcode/common/devpath.c` derives the Developer directory from the running
binary's own location (`<developer_dir>/usr/bin/<tool>`), used by `xcrun`,
`xcodebuild` and `xcode-select` through `mk/with-devpath.mk`. A freshly built
or relocated `build/release/` therefore works with no configuration and no
`DEVELOPER_DIR`:

```
$ build/release/usr/bin/xcrun --find strip
.../build/release/Toolchains/XcodeDefault.toolchain/usr/bin/strip
$ build/release/usr/bin/xcrun lipo -info /bin/ls
Architectures in the fat file: /bin/ls are: x86_64 arm64e
```

Every `/opt/xnuports/...` path is gone from our sources. The compiled-in
fallbacks are now `/Library/Developer/CommandLineTools` and
`/usr/local/etc/xcrun.ini`, consulted only when the self-derived directory is
not a Developer layout.

**Layout note.** Our `xcrun` still speaks the older layout —
`<dev>/SDKs/<n>.sdk` and `<dev>/Toolchains/<n>.toolchain`, with `info.ini`
metadata — while modern Xcode has neither path: SDKs live under `Platforms/`
and the toolchain is `.xctoolchain`. `bundles` therefore also emits
compatibility symlinks and an `info.ini` inside each bundle. That is a bridge,
not the destination: teaching `xcrun` and `xcodebuild` to read
`SDKSettings.plist` from the Apple layout is stage 4 work. Nothing in Apple's
tree is displaced by the symlinks.

`configs/` and `scripts/` are now consumed by `bundles` rather than being
orphaned: `configs/xcrun.ini` seeds the emitted defaults, and the `scripts/`
shims are installed into the toolchain's `usr/bin`. They resolve `xcrun`
relative to their own installed location, so they carry no absolute path.
`configs/Makefile` and `scripts/Makefile` were removed — both were unreachable
installers keyed to the old `PREFIX`.

Three bugs fell out of making the tree actually usable:

1. **`xcrun` crashed with SIGTRAP on any long `PATH`.** `call_command()` built
   its environment with `sprintf` into `PATH_MAX` buffers, and the `PATH=`
   entry appends the caller's entire `PATH` to two absolute directories —
   over roughly 840 characters it overflowed, which `_FORTIFY_SOURCE` turns
   into a trap at `-O2` and silent corruption at `-O0`. The entries are now
   allocated to fit with `asprintf`.
2. **Uninitialised environment entries.** The same function `malloc`'d slots
   for `TARGET_TRIPLE` and the deployment target and passed them to `execve`
   even when neither was set, handing the child garbage. Entries with no value
   are now simply left out.
3. **A failing tool vanished silently.** `src/Makefile` prefixes each
   recursion with `-` so one broken tool does not stop the rest — but nothing
   then noticed the gap, and a `-Werror` slip removed `xcrun` from the release
   tree with a clean-looking build. `bmake check` now fails if any inventory
   entry did not produce its binary.

### Stage 4 — Port the closed-source utilities

Our own BSD-licensed reimplementations, in `src/xcode/`, landing on the same
`PROGS+=` / `tool.d/` machinery as everything else — no build-system cost.

**The `/usr/bin/xc*` family first**, since it is the most conspicuous gap.
Enumerated from a stock Xcode `Developer/usr/bin`:

| Tool | Status |
|------|--------|
| `xcodebuild` | have (orchestration only — no compilation) |
| `xctrace` | have (stub — no tracing backend) |
| `xccov` | missing — parse LLVM coverage data, report text/JSON/HTML |
| `xcresulttool` | missing — bundle/unbundle test results, export attachments |
| `xcstringstool` | missing — `.xcstrings` catalog processing |
| `xcsigningtool` | missing — signing identity management, keychain |
| `xctest` | missing — XCTest bundle runner |
| `xcdebug` | missing |
| `xcindex-test` | missing |
| `xcdevice` | missing |
| `xcdiagnose` | missing |

Then the rest of section 6 in its existing priority order: `actool`, `momc`,
`coremlc`, `ibtool`, `stapler`, `agvtool`, `altool`/`iTMSTransporter`.

Also in this stage: deepen the tools we already have — `codesign` CMS/
certificate signing, `notarytool` history/log/cancel, `devicectl` app install
and file/log transfer, `simctl` io/push/location.

### Stage 5 — Components with their own build system (engine done)

`mk/port.mk` is the second engine, the counterpart to `mk/tool.mk`: it *drives*
a component's own build rather than compiling sources itself. `mk/ports.mk` is
the inventory, `ports/Makefile` the driver, `mk/port.d/<name>.mk` the per-port
fragments — the same shape as the tool side.

Phases are configure → build → stage, each with a stamp file, and the staged
prefix is copied into the release tree so *we* choose what ships rather than the
port's own install rules. Knobs: `P_PROGS`, `P_CONFIGURE`, `P_CONFIGURE_ARGS`,
`P_MAKE`, `P_MAKE_ARGS`, `P_LINKS`, `P_PREPARE`, `P_COPY`, `P_NOBUILD`.

Ports are gated behind `MK_PORTS`, off by default — each runs a full configure
and make, which costs more than the rest of the tree together.

```sh
bmake MK_PORTS=yes        # build the ports too
bmake list-ports          # the port inventory
bmake check MK_PORTS=yes  # verify every port produced its binary
bmake clean               # keeps build/ports -- a port can cost hours
bmake clean-ports         # remove the port work directories
bmake distclean           # remove build/ entirely
```

**`P_COPY` defaults to yes**, meaning the source is copied into
`build/ports/<name>/src` and built in tree. The submodule is never written to
either way, but these old autoconf trees have rules that only work in tree —
out of tree they stop at "No rule to make target 'alloca_.h'" or silently drop
objects. `P_PREPARE` then runs inside that private copy: the ports-style
post-extract step.

Building today:

| Port | Version | Matches Apple |
|------|---------|---------------|
| `gperf` | 3.0.3 | yes |
| `flex` | 2.6.4 | yes |
| `gnumake` (as `make` + `gnumake`) | 3.81 | yes |
| `llvm` — clang 21.1.6, `libtapi.dylib`, plus `llvm-nm`, `llvm-otool`, `llvm-objdump`, `llvm-size`, `llvm-strings`, `llvm-dwarfdump`, `llvm-cov`, `llvm-profdata`, `dsymutil` | 21.1.6 | our own build |

`llvm` is the port everything else waits on, and by far the longest —
most of an hour on ten cores, which is the main reason `MK_PORTS` is off by
default. It is configured for the two targets Apple ships on this hardware,
with tests, docs, examples, benchmarks and bindings off; the build directory
comes to 2.7 GB. `P_NOSTAGE` takes the wanted binaries straight out of the
build tree, because LLVM's install target writes gigabytes of headers and
libraries we do not ship.

Once `llvm-nm` and `llvm-otool` exist, `bundles` links `nm` and `otool` at
them, matching a stock toolchain (where the cctools builds live alongside as
`nm-classic` and `otool-classic`).

`libtapi.dylib` is staged to the toolchain's `usr/lib` via `P_LIBS`, since ld64
links against it. **`ports` runs before `progs`** in the top-level `all` for
exactly that reason.

What each needed, all of it non-obvious:

- **flex** — its bundled libtool links `libfl` as a dylib with
  `-flat_namespace`, which modern ld refuses outright for shared-cache-eligible
  dylibs. Nothing here wants the library, so `--disable-shared`.
- **gnumake** — `job.c` and `remake.c` call `general_vpath_search()` and
  `allocated_vpath_expand_for_file()`, which live in `next.c`, an Apple/NeXT
  source the autoconf Makefile never lists because Apple builds this from their
  own project file. `P_PREPARE` appends it to the source and object lists. The
  port also installs its binary as `make`, not `gnumake`; Apple ships both, so
  `P_LINKS` supplies the second name.
- **tapi — `libtapi.dylib` builds, and ld64 links against it.** tapi builds
  only inside the LLVM tree (`llvm_add_library`, `CLANG_SOURCE_DIR`), so it is
  an `LLVM_EXTERNAL_PROJECTS` entry, from a copy that `mk/port.d/llvm.mk`
  patches. Three scripts under `mk/scripts/` handle the mechanical drift:

  | Script | Drift it repairs |
  |---|---|
  | `tapi-shim-linker-flag.sh` | `llvm_check_linker_flag()` — the LLVM CMake module is gone; forwarded to CMake's `check_linker_flag` |
  | `tapi-fix-diagnostics.sh` | clang-tblgen now rejects diagnostics that start with a capital or end in punctuation; 8 messages reworded minimally |
  | `tapi-modernize-llvm-api.sh` | `startswith`/`endswith`/`equals` → `starts_with`/`ends_with`/`==`, `getDirectory`/`getFile` → the `getOptional*Ref` spellings, and the `TapiUniversal::create` stub signature |

  What the scripts cannot honestly express lives in `mk/patches/tapi/`, applied
  after them so patches are written against the post-rename tree — the ports
  tradition. The first restructures `InterfaceFileManager`'s error handling:
  `getOptionalFileRef` returns a `CustomizableOptional` where the old
  `getFile` returned an `ErrorOr`, so `.getError()` has no meaning any more.

  Two further points, both deliberate:

  - **`LINKER_SUPPORTS_NO_INITS` is forced off.** tapi asks for
    `-Wl,-no_inits`, which Apple wants because the linker dlopens libtapi and
    they want no static-initializer cost. Against LLVM 21 that link fails —
    two dozen LLVM objects now carry initializers. Nothing here needs the
    property, so the check is answered in the negative rather than the flag
    fought.
  - **The port builds named targets, not `all`.** With tapi in the tree, `all`
    also builds the tapi CLI tool and its APIVerifier/Frontend libraries, which
    carry drift beyond what the scripts and patches cover (clang's
    `DiagnosticOptions` is no longer reference-counted). `libtapi` itself — the
    only part ld64 needs — builds clean, so `P_MAKE_ARGS` names exactly what we
    ship.

- **gm4, bison** — not enabled. Both restore their missing gnulib templates
  fine (`mk/scripts/gnulib-restore-templates.sh` recreates `alloca_.h` and
  `getopt_.h`, which Apple's drops ship the *outputs* of but not the inputs),
  and then their vendored gnulib — two decades older than the SDK — substitutes
  its own `<stdint.h>`/`<inttypes.h>` and the system `_inttypes.h` stops seeing
  `intmax_t`. Fixing that means forcing configure to accept the system headers
  or refreshing the vendored gnulib. Their fragments are in place.

Apple also ships `lex`, `yacc` and `m4` in the toolchain, but as distinct
binaries rather than links to flex/bison/gm4, so `P_LINKS` does not cover them.

**Still ahead on this stage**, in dependency order:

| Tree | Build system | Notes |
|------|--------------|-------|
| `llvm-project`, `swift` | CMake | clang and swiftc — the headline deliverables. Set `P_COPY=no`: CMake builds out of tree properly, and copying LLVM is not worth the disk. |
| `tapi` | CMake, inside the LLVM tree | **unblocks ld64**, which already compiles completely |
| `cpython` (+ `python-apple-support`) | autoconf | |
| `git` | autoconf + GNU make | |
| `objc4` | Xcode project | builds a dylib — needs a library target, not a program target |
| `libgit2` | CMake | |

Landing llvm/swift is what lets `xcodebuild` stop delegating and actually
compile, and what fills `${XCTOOLCHAIN}/usr/bin`.


## 10. Development Workflow

### Building

```sh
bmake                   # build everything, then emit the bundles
bmake check             # verify every inventory entry produced a binary
bmake bundles           # re-emit the .xctoolchain / .sdk metadata only
bmake list-progs        # print the inventory with install locations
bmake clean             # remove build/
bmake MK_TOOLCHAIN=no   # skip the binutils tier (cctools, ld64)
```

Run `bmake check` after any build you care about: `src/Makefile` ignores
per-tool failures on purpose, so without it a broken tool just disappears.

To build a single program without the whole tree, invoke the engine directly
the way `src/Makefile` does:

```sh
bmake -f mk/tool.mk TOP=$PWD T_DIR=xcode/codesign T_PROG=codesign T_BIN=usr/bin
```

### Testing

For codesign specifically (our most tested tool):

```sh
cc test.c -o test_bin
build/release/usr/bin/codesign -f -s - test_bin
codesign --verify --strict test_bin
```

### Adding a New Tool

1. Put the sources somewhere under `src/`. For our own reimplementations that
   is `src/xcode/<tool-name>/`; for an imported component it is wherever the
   submodule already keeps them — do not move or copy them.
2. Add one line to `mk/progs.mk`:
   ```
   PROGS+=	xcode/<tool-name> <tool-name> usr/bin
   ```
   The third field is the path under `build/release/`, mirroring where Xcode
   ships the tool: `usr/bin`, `usr/libexec`, or `${XCTOOLCHAIN}/usr/bin`.
3. **Only if it needs flags**, add `mk/tool.d/<tool-name>.mk`. Sources are
   discovered automatically, so a plain tool needs no fragment at all:
   ```makefile
   # what the tool links against, and why
   .include "${TOP}/mk/with-openssl.mk"
   T_LDADD+=	-lz -framework Security
   ```
   Recognized knobs: `T_SRCS` (override the discovered source list; entries may
   be TOP-relative paths for sources outside the tool's directory), `T_CFLAGS`,
   `T_LDADD`, `T_LINKS` (extra hardlinked names), `T_SCRIPT` (install a script
   instead of compiling), `T_NOBUILD` (skip).
4. Test: `bmake && build/release/usr/bin/<tool-name> --help`

There is no per-tool Makefile to write, and nothing to register in a `SUBDIRS`
list.

### bmake Gotchas

| GNU Make | bmake Equivalent | Notes |
|----------|------------------|-------|
| `$(CURDIR)` | `${.CURDIR}` | Returns absolute path in bmake |
| `$(shell cmd)` | `VAR != cmd` | Use `!=` for command substitution |
| `$<` | `${.IMPSRC}` / name the source | Empty in bmake explicit rules |
| `%.o: %.c` | `.for` loop | bmake doesn't support path-prefixed patterns |
| `?=` for `CC`, `CFLAGS`, `CXXFLAGS` | plain `=` | bmake predefines all three in its own `sys.mk`, so `?=` is silently a no-op |

## 11. Testing Requirements

Every tool must pass:
1. **Build test:** `bmake` succeeds without errors or warnings
2. **Inventory test:** every entry in `mk/progs.mk` produces a binary at its
   declared `build/release/<suffix>/<prog>` path (`bmake list-progs`)
3. **Binary verification:** all binaries are valid Mach-O (use `file`)
4. **Functionality test:** tools produce expected output for basic operations
5. **Clean test:** `bmake clean` removes all build artifacts
6. **Read-only test:** nothing is written into the source tree or any
   submodule. After a full build, both of these must be empty:
   ```sh
   git status --porcelain
   git submodule foreach --recursive --quiet 'git status --porcelain'
   ```
7. **Reproducibility:** two clean builds of identical sources produce
   byte-identical binaries. This is section 12 rule 6, and it only holds
   because of `-Wl,-reproducible` (see section 8.1).
   ```sh
   bmake clean && bmake && shasum build/release/usr/bin/* > /tmp/r1
   bmake clean && bmake && shasum build/release/usr/bin/* > /tmp/r2
   diff /tmp/r1 /tmp/r2
   ```

For specific tools:
- **codesign:** must pass `codesign --verify --strict`. Note that
  `spctl --assess` is only meaningful on a machine where Gatekeeper assessment
  is enabled; check for `override=security disabled` in its output before
  treating a pass as evidence.
- **xcrun:** must find and execute tools in the Developer directory
- **xcodebuild:** must parse `.pbxproj` and `.xcconfig` files correctly
- **pkgbuild:** must produce valid `.pkg` archives (test with `pkgutil`)
- **productbuild:** must produce valid distribution packages
- **simctl:** must list available simulators (requires Xcode Simulator installed)
- **cctools:** compare against Apple's counterparts on identical input — the
  function-to-function parity requirement. Compare against the matching name
  (`nm-classic` against `nm-classic`, not against `nm`, which is llvm-nm).
  Invoke both as `./tool` from their own directory, since these tools print
  `argv[0]` verbatim in usage and error messages.

  As of stage 2a, every output comparison matches exactly: `lipo -info`,
  `-detailed_info`, `-thin`, `-extract`, `-create`; `otool-classic -h/-l/-L/-tV`;
  `nm-classic`; `size-classic`; `strings`; `vtool -show`; and every usage and
  error string.

  Round-trips that *rewrite* a binary (`strip -S`, `strip -x`,
  `install_name_tool`) are byte-identical only after
  `codesign --remove-signature` is applied to both. That is expected and not a
  defect: Apple's builds regenerate the ad-hoc signature through
  `libcodedirectory`, which we deliberately do not link (see section 9,
  stage 2a). The Mach-O content itself is identical.
- **ld64:** same approach, once it builds.

## 12. License Compliance Checklist

All code must comply with these rules:
1. Our reimplemented tools (in `src/xcode/`) are BSD-3-Clause
2. Each submodule retains its own license
3. No proprietary Apple code (except what's in open-source submodules)
4. No copying of Apple's closed-source binaries
5. All third-party dependencies must have compatible licenses
6. Build system must be reproducible from source alone

---

## 13. Key Files to Know

| File | Purpose |
|------|---------|
| `docs/DOCUMENTATION.md` | Full audit of our tools vs. Apple's |
| `docs/CLAUDE.md` | This file — development instructions |
| `src/xcode/codesign/cs_sign.c` | Code signing entry point (most tested) |
| `src/xcode/codesign/cs_macho.c` | Mach-O parsing and __LINKEDIT updates |
| `src/xcode/codesign/cs_blob.c` | CodeDirectory/SuperBlob construction |
| `src/xcode/xcodebuild/xcodebuild.c` | Main build orchestration logic |
| `src/xcode/xcodebuild/project.c` | .pbxproj parser |
| `src/xcode/xcodebuild/plist.c` | Property list parser |
| `src/xcode/xcrun/xcrun.c` | Tool location and execution |
| `Makefile` | Top-level bmake build |
| `mk/progs.mk` | The program inventory — add a tool here |
| `mk/tool.mk` | The per-program build engine |
| `mk/xcodetools.sys.mk` | Global flags and tier gating |
| `mk/bundle.mk` | Emits the `.xctoolchain` / `.sdk` bundle metadata |
| `mk/port.mk` | Driver for components with their own build system |
| `mk/ports.mk` | The port inventory |
| `src/xcode/common/devpath.c` | Finds our Developer directory at runtime |
| `configs/xcrun.ini` | Default SDK/toolchain config |

---

## 14. Quick Start for New Contributors

```sh
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/xnuports/xcode-tools.git
cd xcode-tools

# 2. Initialize nested submodules (dist-dev-tools has sub-submodules)
git submodule update --init --recursive

# 3. Build all tools we can build right now
bmake

# 4. Check results
bmake list-progs
ls build/release/usr/bin/

# 5. Clean up
bmake clean
```

`build/release/` is the product; there is no `install` target.

---

## 15. When Asking Claude for Help

Include this information in your request:
1. Which tool or component you're working on
2. What phase of the roadmap you're targeting
3. Whether you need bmake or CMake build integration
4. Whether you need to modify existing Makefiles or create new ones
5. Any specific Apple tool behavior you're trying to match

This document is the single source of truth. Everything you need to know
about the project's current state, goals, and roadmap is in here.
