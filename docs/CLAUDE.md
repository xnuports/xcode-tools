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
│   ├── apple-oss-distributions/    # Apple open-source components
│   │   ├── distribution-Developer_Tools/   # Submodule: Apple OSS dev tools (nested submodules)
│   │   │   ├── CoreOSMakefiles/    # Build system makefiles
│   │   │   ├── Git/                # Apple's Git fork (Git-155)
│   │   │   ├── bison/              # GNU Bison
│   │   │   ├── bootstrap_cmds/     # Bootstrap commands
│   │   │   ├── cctools/            # ar, nm, lipo, strip, otool, vtool, install_name_tool, etc.
│   │   │   ├── developer_cmds/     # asa, ctags, indent, lorder, rpcgen, unifdef
│   │   │   ├── flex/               # Fast Lexical Analyzer
│   │   │   ├── gm4/                # GNU M4
│   │   │   ├── gnumake/            # GNU Make
│   │   │   ├── gperf/              # Perfect hash function generator
│   │   │   ├── headerdoc/          # headerdoc2html, hdxml2manxml, gatherHeaderDoc
│   │   │   ├── ld64/               # Apple linker (ld, ld-classic)
│   │   │   ├── libgit2/            # Git library
│   │   │   ├── pb_makefiles/       # Project Builder makefiles
│   │   │   └── tapi/               # Text-based API tool
│   │   └── objc4/                  # Submodule: Objective-C runtime
│   ├── cctools-helpers/            # OURS: reimplemented libcctoolshelper piece
│   ├── extras/                     # Extra tools Apple's XC does not ship
│   │   ├── bmake/                  # Submodule: NetBSD make (this project's build tool)
│   │   └── bsdmake/                # Submodule: Apple's BSD make
│   ├── git/                        # Submodule: Apple's Git-155 (upstream)
│   ├── openxc-tools/               # Open-source reimplementations of Apple's tools
│   │   ├── openxc/                 # OUR reimplemented Xcode tools (10 tools)
│   │   │   ├── common/             # OURS: shared helpers (devpath.c)
│   │   │   ├── codesign/           # ad-hoc signing & verification (7 files)
│   │   │   ├── devicectl/          # device management (2 files)
│   │   │   ├── notarytool/         # notarization client (6 files)
│   │   │   ├── pkgbuild/           # package building (6 files)
│   │   │   ├── productbuild/       # product building (7 files)
│   │   │   ├── simctl/             # simulator control (5 files)
│   │   │   ├── xcode-select/       # developer dir selection (1 file)
│   │   │   ├── xcodebuild/         # build orchestration (6 files)
│   │   │   ├── xcrun/              # tool locator & executor (3 files)
│   │   │   └── xctrace/            # trace recording (4 files, stub)
│   │   ├── pngcrush/               # Submodule: pngcrush v1.8.1
│   │   ├── agvtool/                # OURS: apple-generic versioning (1 file)
│   │   ├── genstrings/             # OURS: .strings extraction (1 file)
│   │   ├── TextureAtlas/           # OURS: SpriteKit atlas compiler (4 files)
│   │   ├── xarsigner/              # OURS: detached xar signing (1 file)
│   │   ├── TextureConverter/       # OURS: all five modes, 27 formats
│   │   └── vmmap/                  # Submodule: third-party vmmap implementation
│   ├── python/                     # Python runtime sources
│   │   ├── cpython/                # Submodule: CPython v3.14.6 (upstream)
│   │   └── python-apple-support/   # Submodule: Python build system for Apple platforms
│   ├── remorix/                    # Remorix's open-source Apple tools
│   │   └── PlistBuddy/             # Submodule: open-source PlistBuddy
│   └── swiftlang-llvm/             # Swift/LLVM compiler sources
│       ├── llvm-project/           # Submodule: LLVM/Clang/LLDB/MLIR/flang/etc.
│       └── swift/                  # Submodule: Swift compiler
└── build/                          # Generated output
    ├── obj/<dir>/                  # object files
    ├── gen/<tool>/                 # build-time generated sources
    ├── lib/                        # static libraries
    └── release/                    # staged, drop-in Developer/ tree
```

---

## 4. Tools We Have Implemented (10)

Our own BSD-licensed reimplementations in `src/openxc-tools/`:

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

**Sources track upstream, not forks.** `llvm-project` and `swift` come from
swiftlang, the Apple components from apple-oss-distributions (corecrypto from
`apple/corecrypto`), and CPython from python/cpython, each pinned to the tag or
release branch named below. The remaining submodules — `apple_internal_sdk`,
`ld-internals`, `PlistBuddy`, `pngcrush`, `python-apple-support` — stay on
xnuports forks, either because they carry local changes or because there is no
upstream to track.

Component versions are read from each submodule's own git tag at build time
(see `CCTOOLS_VERSION` in `mk/with-cctools.mk` and `LD64_VERSION` in
`mk/tool.d/ld.mk`) rather than written down, because a hardcoded version lies
the moment a submodule is bumped. `distribution-Developer_Tools/release.json`
has exactly that problem and is not used.


These submodules provide source code for tools previously listed as "no source":

| Submodule | Path | Version | Tools Covered |
|-----------|------|---------|---------------|
| `llvm-project` | `src/swiftlang-llvm/llvm-project/` | swift-6.3.3-RELEASE | clang, swift-frontend, llvm-*, lld, lldb, dsymutil, dwarfdump, clang-format, clangd |
| `swift` | `src/swiftlang-llvm/swift/` | swift-6.3.3-RELEASE | swiftc, swift-frontend, swift-driver, SPM, swift-format, sourcekitd |
| `objc4` | `src/apple-oss-distributions/objc4/` | rel/objc4-951 | ObjC runtime (libobjc.A.dylib) |
| `distribution-Developer_Tools` | `src/apple-oss-distributions/distribution-Developer_Tools/` | rel/Developer_Tools-26 | See nested submodules below |
| `git` | `src/git/` | Git-155 | git, git-receive-pack, git-shell, git-upload-pack |
| `cpython` | `src/python/cpython/` | v3.14.6 | python3, pip3, pydoc3, 2to3 |
| `python-apple-support` | `src/python/python-apple-support/` | heads/main | **Reference only** — a meta-build system for Python XCFrameworks. We build Python ourselves, so this is kept for reference rather than used. |
| `PlistBuddy` | `src/remorix/PlistBuddy/` | heads/main | PlistBuddy |
| `pngcrush` | `src/other/pngcrush/` | v1.8.1 | pngcrush |
| `xctoolchain` | `xctoolchain/` | heads/master | **Reference only** — generic `.xcconfig` build settings, not a source of the `.xctoolchain` bundle format |
| `ld-internals` | `include/ld-internals/` | heads/main | ld64 private headers |

### distribution-Developer_Tools Nested Submodules

| Sub-submodule | Path | Version | Tools |
|--------------|------|---------|-------|
| `cctools` | `src/apple-oss-distributions/distribution-Developer_Tools/cctools/` | 1035.1.102 | ar, nm, lipo, strip, otool, vtool, install_name_tool, bitcode_strip, codesign_allocate, ctf_insert, libtool, segedit, etc. |
| `ld64` | `src/apple-oss-distributions/distribution-Developer_Tools/ld64/` | 957.1 | ld, ld-classic |
| `developer_cmds` | `src/apple-oss-distributions/distribution-Developer_Tools/developer_cmds/` | 87 | asa, ctags, indent, lorder, rpcgen, unifdef |
| `headerdoc` | `src/apple-oss-distributions/distribution-Developer_Tools/headerdoc/` | 8.9.32 | headerdoc2html, hdxml2manxml |
| `bison` | `src/apple-oss-distributions/distribution-Developer_Tools/bison/` | 16 | bison |
| `flex` | `src/apple-oss-distributions/distribution-Developer_Tools/flex/` | 35 | flex, flex++ |
| `gnumake` | `src/apple-oss-distributions/distribution-Developer_Tools/gnumake/` | 136 | make, gnumake |
| `gperf` | `src/apple-oss-distributions/distribution-Developer_Tools/gperf/` | 15 | gperf |
| `gm4` | `src/apple-oss-distributions/distribution-Developer_Tools/gm4/` | 19 | gm4 |
| `tapi` | `src/apple-oss-distributions/distribution-Developer_Tools/tapi/` | 1600.0.11.8 | tapi |
| `pb_makefiles` | `src/apple-oss-distributions/distribution-Developer_Tools/pb_makefiles/` | 1009 | Build system makefiles |
| `bootstrap_cmds` | `src/apple-oss-distributions/distribution-Developer_Tools/bootstrap_cmds/` | 138 | Bootstrap commands |
| `CoreOSMakefiles` | `src/apple-oss-distributions/distribution-Developer_Tools/CoreOSMakefiles/` | 79 | Build system infrastructure |
| `Git` | `src/apple-oss-distributions/distribution-Developer_Tools/Git/` | 155 | Apple's Git fork |
| `libgit2` | `src/apple-oss-distributions/distribution-Developer_Tools/libgit2/` | 30 | Git library |

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
| xcsigningtool | Code Signing | ❌ Not reimplementable — a client for Apple's cloud signing service, see below |
| stapler | Code Signing | ❌ No source (Apple proprietary) |
| embeddedBinaryValidationUtility | Code Signing | ❌ No source (Apple proprietary) |
| xccov | Testing | ✅ Ours, `src/openxc-tools/xccov/` (`view --report`) |
| xcresulttool | Testing | ✅ Ours, `src/openxc-tools/xcresulttool/` (`get object`) |
| xcstringstool | Localization | ❌ No source (Apple proprietary) |
| xctest | Testing | ❌ No source (Apple proprietary) |
| xed | IDE | ❌ GUI app, not a CLI tool |
| xcdebug | Debug | ❌ No source (Apple proprietary) |
| xcindex-test | Debug | ❌ Not reimplementable — drives Xcode's build service, see below |
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
| xml2man | Docs | ✅ Source: `src/apple-oss-distributions/distribution-Developer_Tools/headerdoc/xmlman/` — built |
| agent, ba-package, ba-serve | Build Assistant | ❌ No source (Apple internal) |
| backgroundassets-debug | Debug | ❌ No source (Apple internal) |
| compositeMD5 | Archive | ❌ No source (Apple internal) |
| convertRichTextToAscii | Conversion | ❌ No source (Apple internal) |
| filtercalltree | Debug | ❌ No source (Apple internal) |
| resolveLinks | File | ✅ Source: `src/apple-oss-distributions/distribution-Developer_Tools/headerdoc/xmlman/` — built |
| swinfo | File | ❌ No source (Apple internal) |
| stringdups | File | ❌ No source (Apple internal) |
| extractLocStrings | Localization | ❌ No source (Apple proprietary) |
| gatherheaderdoc | Docs | ✅ Available via distribution-Developer_Tools/headerdoc (as `gatherHeaderDoc.pl`) |
| unifdef | Dev command | ✅ Source: `src/apple-oss-distributions/distribution-Developer_Tools/developer_cmds/unifdef/` |
| c89, c99 | Compatibility | ✅ Covered by clang (aliased) |
| metal, metal-package-builder | Graphics | ❌ No source (Apple proprietary) |
| mig | IPC | ❌ No source (not in open-source releases) |
| unwinddump | Debug | ❌ No source (Apple proprietary) |

### 6.2 Resource Fork Tools (`Developer/Tools/`)

Apple ship no source for any of these.  Retro68 has a Rez, but it is GPLv3,
needs Boost and a bison newer than the one this tree has, is missing the
ResourceFiles library it links against, and covers only Rez of the six --
so these are ours, written against the behaviour of Apple's binaries.

| Tool | Description | Status |
|------|-------------|--------|
| DeRez | Resource de-compiler | ✅ Ours, `src/openxc-tools/Rez/` (data statements; type-directed output waits on Rez) |
| Rez | Resource compiler | ✅ Ours, `src/openxc-tools/Rez/` (`data`/`read`; `type`/`resource` not yet) |
| ResMerger | Resource merger | ✅ Ours, `src/openxc-tools/Rez/` |
| GetFileInfo | File metadata query | ✅ Ours, `src/openxc-tools/Rez/` |
| SetFile | File attribute setter | ✅ Ours, `src/openxc-tools/Rez/` |
| SplitForks | Fork splitter | ✅ Ours, `src/openxc-tools/Rez/` |

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
| swift-plugin-server | Swift | ✅ Source in `src/swiftlang-llvm/swift/` |
| swift-stdlib-tool | Swift | ✅ Source in `src/swiftlang-llvm/swift/` |
| swift-static | Swift | ❌ Apple proprietary |
| swift-symbolgraph-extract | Swift | ✅ Source in `src/swiftlang-llvm/swift/` |
| swift-synthesize-interface | Swift | ✅ Source in `src/swiftlang-llvm/swift/` |

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
- `-Werror` applies only to our own sources under `src/openxc-tools/`; imported
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
     `src/swiftlang-llvm/llvm-project/llvm/include`.
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
`@(#)PROGRAM:ld  PROJECT:ld64-957.1`.

**`-arch arm64` works, and the fix was a tapi bug, not something needing
ld-prime.** Both arm64 and arm64e link and run, and the arm64 output carries the
same load-command sequence and the same `libSystem.B.dylib` dependency as
Apple's `ld-classic` produces.

The chain is worth recording, because the symptom pointed nowhere near the
cause. Current macOS SDKs declare `libSystem.tbd`'s targets and reexports as
`[x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst]` — no
`arm64-macos` at all. ld64 asks tapi for a slice, and only sets tapi's
`ExactCpuSubType` flag when the linker is enforcing dylib sub-type matching —
which for arm64 it is not: `Options.cpp` leaves `fEnforceDylibSubtypesMatch` at
its default of `false` for `CPU_TYPE_ARM` and `CPU_TYPE_ARM64`. So fuzzy
matching is what ld64 asked for, and tapi's `getArchForCPU` did not implement
it: on a miss it returned the *requested* architecture unchanged, a slice the
file does not contain, and every symbol lookup against it came up empty. The
failure is silent — the `.tbd` loads, `ld -t` lists it, and then every symbol is
undefined.

`mk/patches/tapi/0002-getArchForCPU-accept-a-compatible-slice.patch` scans for
any slice of the same CPU type, which is exactly what "sub-types need not match"
means, and cannot fire when `ExactCpuSubType` is set.

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
`src/openxc-tools/common/devpath.c` derives the Developer directory from the running
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

Our own BSD-licensed reimplementations, in `src/openxc-tools/`, landing on the same
`PROGS+=` / `tool.d/` machinery as everything else — no build-system cost.

**The `/usr/bin/xc*` family first**, since it is the most conspicuous gap.
Enumerated from a stock Xcode `Developer/usr/bin`:

| Tool | Status |
|------|--------|
| `xcodebuild` | have (orchestration only — no compilation) |
| `xctrace` | have (stub — no tracing backend) |
| `xccov` | `view --report [--json]` done; the tabular view, `diff` and `merge` remain |
| `xcresulttool` | `get object` done; `get test-results`, `export`, `merge`, `compare` remain |
| `xcstringstool` | missing — `.xcstrings` catalog processing |
| `xcsigningtool` | not attempted, deliberately — see below |

#### Why actool, ibtool, ictool and ibtoold are not reimplemented

`actool`, `ibtool` and `ictool` are one binary shipped under three names --
the same 92 KB client, differing only in the program name string, and all
three report their errors under the `com.apple.actool.errors` key.  None of
them does any work.  Each one finds `ibtoold`, daemonises it, writes its
argument vector down a pipe and reads back an exit status; the strings
`Could not locate ibtoold.` and `ibtool read exit status %ld` are the whole
job.  Reimplementing the client is a couple of hundred lines and buys
nothing, because the tool it starts is where the work is.

`ibtoold` links AssetCatalogFoundation, AssetCatalogKit, CoreUI,
IDEInterfaceBuilderKit, IBAutolayoutFoundation, IDEFoundation, IDEKit,
DVTFoundation and Cocoa, and imports 395 symbols including 102 Objective-C
classes.  That is a different measurement from the one taken for xccov and
xcresulttool, where 37 and 42 imported symbols turned out to be Apple's
internal factoring in front of a file format we could read directly.  Here
the imports are the implementation: interface builder documents are compiled
by IDEInterfaceBuilderKit, their constraints solved by
IBAutolayoutFoundation, and asset catalogues written by CoreUI.

There is a real format behind `actool` -- `Assets.car` is a BOMStore, and
compiling one tiny 32x16 PNG produces 14 KB of it -- and reading and writing
that is a bounded reverse-engineering project.  It is simply a much larger
one than any tool in this tree so far: the rendition keys, the per-idiom,
per-scale and per-appearance variants and CoreUI's packed image encodings
are each their own format.  `ibtool` has no such bottom: compiling a
storyboard means knowing the archived representation of every AppKit and
UIKit class that can appear in one.

So these are not documented as impossible, the way xcsigningtool is.  They
are the two biggest single pieces of Xcode that this tree does not have, and
they want to be their own projects rather than an afternoon inside somebody
else's.

#### Why stapler is not reimplemented

It looked like local format work -- attach a notarization ticket to a
bundle, a package or a disk image -- and it is not.  Both of its
subcommands begin by asking Apple for the ticket.

`staple -v` on an unnotarized package says "Cannot download ticket. CDHash
must be set."; `validate -v` on a notarized application prints the request
it makes and the answer it gets:

    Domain is api.apple-cloudkit.com
    .../database/1/com.apple.gk.ticket-delivery/production/public/records/lookup

The record is looked up by the code directory hash, and the ticket comes
back from Apple's ticket-delivery database.  Nothing on this machine
carries a stapled ticket to read the format out of either: every signed
application in /Applications has an ordinary _CodeSignature/CodeResources
and no ticket beside it, because Gatekeeper resolves them online.

So there is no local artefact to reverse and nothing to check an
implementation against without a notarized product whose ticket Apple still
serves.  Our notarytool submits to the same service and can report what it
says; stapling what comes back is the part that cannot be verified here.

#### Why cktool is not reimplemented

Every one of its subcommands is a request to Apple's servers.  `save-token`
puts an App Store Connect key in the keychain, and `export-schema`,
`import-schema`, `create-record`, `query-records` and the rest each turn
into an authenticated call to the CloudKit web service; the answers are
whatever that service returns.  It is the same shape as xcsigningtool: no
local format to read instead of the framework, and nothing checkable against
Apple's binary without an Apple developer account and their servers
answering.

#### TextureConverter: the compressor ports

At 7 MB it is the largest tool in Developer/usr/bin, and almost all of that
is other people's compressors.  Apple name six selectable back ends -- ARM,
ETC2COMP, ISPC, NVTT, PVRTC and STB -- and offer ASTC in every block size,
BC1 through BC7, EAC, ETC2 and PVRTC.  Writing those from scratch would be
writing five worse texture compressors, so the encoders are ports and the
container, the CLI and the uncompressed paths will be ours.

Four are in and building, each pinned and each its own submodule:

| Port | Version | Licence | Formats |
|------|---------|---------|---------|
| `extras/astc-encoder` | 5.7.0 | Apache-2.0 | ASTC, every block size, LDR and HDR |
| `extras/stb` | stb_dxt.h v1.12 | MIT / public domain | BC1, BC3, BC4, BC5 |
| `extras/etc2comp` | commit 39422c1a | Apache-2.0 | EAC_R11, EAC_RG11, EAC_RGBA8, ETC2_RGB8, ETC2_RGB8A1 |
| `extras/nvidia-texture-tools` | 2.1.2 | MIT + Apache-2.0 | BC6H, BC7 |

Between them that is every format Apple's tool can write except PVRTC.
Two of the four needed work to build at all, and neither was patched --
submodule contents are left alone, so both are handled from the command
line in mk/port.d/:

  - etc2comp's CMakeLists asks for cmake 2.8.9, which cmake has refused
    since 4.0, and its EtcLib cannot be configured alone because it
    declares no minimum.  Its fifteen sources are compiled directly.
  - NVTT 2.1.2 predates Apple Silicon: nvcore/Debug.h tests a macro
    (NV_CPU_AARCH64) that nvcore.h never defines, so pointer checks took
    a 32-bit branch that truncates, and the crash handler reaches
    #error "Unknown CPU" on arm64.  Two defines settle both.

Two back ends are not ported:

  - **ISPC** is Intel's ispc_texcomp, and it offers nothing this tree
    cannot already write: BC1-BC7, ASTC and ETC2_RGB8 are covered above.
    It is a *selectable* back end rather than a unique capability, and it
    needs the ISPC compiler -- an LLVM-based toolchain of its own -- to
    build.  Worth adding when `--compressor=ISPC` has to answer as Apple's
    does; not worth it for coverage.
  - **PVRTC** has no open encoder.  NVTT bundles Imagination's PVRTexTool
    as prebuilt x86 libraries, which is neither source, nor arm64, nor a
    licence this tree can carry.  Apple's own help says PVRTC in Metal is
    deprecated and recommends ASTC, ETC2 or BC instead.

The tool itself is in `src/openxc-tools/TextureConverter`, and all five
modes are written: `examine`, `convert`, `compress`, `decompress` and
`compare`, and all four output formats: KTX, KTX2, `.h` and DDS.  It is gated on `MK_PORTS`, since it links the encoders it
drives.

Compression is byte-identical to Apple's across all twenty-seven formats --
853 of 855 combinations of two images and sixteen option sets.  The two are
BC7 at Fastest, which Apple send to ISPC; there is no ISPC port here and the
tool says so rather than encoding with something else.  Which encoder each
format goes to is a table rather than a search: ASTC to ARM's, every BC
format to NVTT except BC1 at Highest which goes to STB, and every ETC2 and
EAC format to ETC2COMP.

The thirteen uncompressed formats go to no encoder at all -- Apple call
that compressor RAW -- and are byte-identical too, 546 of 546 over both
containers, seven images and all three wrap modes.  Two measurements
settled the packing: a byte is the sample rounded to a sixteen bit unorm
and reduced to its top byte, which only the mip levels can tell from
either one-step rule since every sample the decompress path produces is
already a multiple of 1/255; and a half rounds a tie up rather than to
even, so 0.4659423828125 comes out one step above what the hardware
conversion gives.

`--srgb_format` is byte-identical over the twenty-one formats that have an
sRGB spelling.  It changes the OpenGL and Vulkan enumerants, moves the
Metal one eighteen down, sets the version 2 transfer function to 2, and
sets the linear bit on whichever sample is alpha.  The formats that carry
no colour have no sRGB spelling and Apple's tool segfaults on all five of
them; this prints an error.

`decompress` is byte-identical for all 53 cases, in both containers.  Two
formats are refused because NVTT cannot read them -- BC7, whose decoder is
the old avpcl prototype and fails an assertion on a conforming block, and
ETC2_RGB8A1, whose call site in `nvtt/Surface.cpp` is commented out and
marked "@@ Not implemented".  EAC_R11 and EAC_RG11 are commented out there
too and are decoded in `eac.c` instead.

`compare` matches wherever Apple's own compare is self-consistent, which is
not everywhere.  Their compare does not read a compressed container through
their own decompressor: a 64x64 BC1 file measured against an ASTC file
answers 603.47 when the BC1 is decompressed first and 639.19 when it is not.
The same split shows on BC4, ETC2_RGB8 and the EAC formats, and on BC5 it
surfaces as `PSNR:1.79...e308` from an unguarded divide by zero.  Ours reads
every container through the decoders the rest of the tool uses, so its
compare and its decompress agree with each other.

All four output formats are selected by the output path's extension and by
nothing else -- `--file_format` reaches none of them in Apple's tool
either -- and all four are byte-identical across `convert`, `compress` and
`decompress`: 4556 of 4560 over four containers, twenty-four formats, six
images and nine option sets, excluding the 144 combinations where their
own tool crashes.

The `.h` output is the one place `--gamut_out` leaves a mark: no container
records it.  It is also the only place a format is named rather than
enumerated, and the names are not derivable from Apple's own -- BC6U is
`atcFormatBc6Uf16` -- so they are tabulated in `formats.c`.

DDS is NVTT's, down to the "NVTT" signature and version 2.1.2 in the
reserved words, and always names its format through the DX10 header.  A
format Direct3D has no DXGI enumerant for gets no DDS: their tool
compresses it, says so on stderr, writes nothing and exits zero.

What is left over are encoder tie-breaks on images whose mip levels are
not a multiple of the block: a handful of ASTC blocks and, in one case,
two bytes of a BC5 file.  Level 0 is identical every time; the float chain
feeding the level that differs is identical; and feeding that level's
exact floats back through Apple's tool on its own reproduces their blocks.
It is not the astcenc version -- 5.4.0, 5.6.0 and 5.7.0 all differ in the
same places -- nor the instruction set, since a scalar build differs
identically.  Which candidate an equal-error search settles on is not
something the calling code decides.

`--wrap_mode` says what the mip filter reads past an edge and is NVTT's
wrap mode passed straight through.  Mirror is the default, which is why
nothing noticed it was missing for so long: the chain was already
Mirror's.

The pixel path runs in this order, and every step of it was measured
rather than assumed:

  1. read the image
  2. `--flip_x`, `--flip_y` (`--flip_z` is accepted and does nothing)
  3. `--gamma_in`, over the colour channels only
  4. `--max_extent`, which is not a resize -- Apple halve with the mipmap
     filter until neither side is longer than the extent, so 64 pixels
     with `--max_extent=12` comes out 8 and is bit for bit the chain's
     third level.  `--resize_filter` and `--resize_round_mode` are
     recorded in TC_Options and change nothing, in their tool as in this
     one.
  5. `--normal_map` normalisation
  6. the mip chain
  7. `--alpha_mode=Premultiply`, base level only
  8. `--gamma_out`, over every level
  9. `--rgbm_encoding`, over every level

`--normal_map` turns off steps 3, 7, 8 and 9: the channels are a
direction rather than a colour, and Apple write the same texels with each
of those as without.  `--rgbm_encoding` turns off step 7 for the same
reason -- alpha stops being coverage once it carries the multiplier.

The gamma is NVTT's, and at exactly 2.2 nvimage does not call powf but a
table-and-polynomial pair good to three parts in a million, so a correctly
rounded powf is wrong in every second sample.  RGBM packs into a range of
six with the multiplier floored at 32/255, which is the floor astcenc's
own documentation recommends for RGBM data, and astcenc is told: its RGBM
map flag, the reconstruction scale, and alpha weighed at twice it.

A gamma of one is skipped rather than applied.  That is not an
optimisation: the exponentiation clamps at zero and the Kaiser filter
undershoots there, so running it would lift every negative sample the
chain produced -- which is what the default gamma of 1.000000 does if the
option is read as present rather than as a value.

`--rgbm_range` is the range the encoding packs into, six unless it is
given and never less than one -- Apple parse it with stof, so a fraction
is allowed, a word is an error of its own, and anything below one is
refused with the usage on stdout.  It reaches the ASTC encoder as
`rgbm_m_scale` and, at twice its value, `cw_a_weight`, which is where the
six came from in the first place.  The complaint only fires when
`--rgbm_encoding` is there to use it: the range alone is accepted and
recorded and does nothing.

`--alpha_to_coverage` is nvimage's, both halves of it.  A mip chain loses
coverage -- filtering an alpha channel that a shader is going to threshold
makes the thresholded area shrink -- so each image's alpha is scaled until
the fraction of it above `--alpha_reference` matches the first image's.
The search is `scaleAlphaToCoverage`'s: bisect over [0, 4] from a start of
one, ten steps, keeping whichever step came closest rather than the last,
which is why the scales are dyadic (1.005859 is one plus 3/512, the tenth
step of a path that goes 1, 2.5, 1.75, 1.375 and halves inward).  The
measure is `alphaTestCoverage`'s: not a count of texels above the
reference but sixteen bilinear samples of each 2x2 of neighbours, which is
what a magnified texture is actually thresholded at, so an image narrower
than two texels has no measure and is left alone.  The reference is 0.95
unless `--alpha_reference` says otherwise, and no other value fits.

An earlier note here said the measure was not nvimage's.  It is; the
mistake was about which image is the reference.  It is the *first* image
and not the first of each face: a cubemap measures face zero's base and
scales the other five faces' bases against it as well as every level
behind them, so level zero of face zero is the only image in the file left
alone.  An array's elements and a volume's slices go the same way, slice
by slice.  Measured against something that scaled per face, every face
past the first looks like a coverage measure that does not fit.

Under `--alpha_mode=Ignore` it is inert without being guarded: alpha is
one everywhere, so every image already covers everything, the search's
first step has zero error and stops there.  The only thing that changes is
the `TC_Options` string.

`--crop_uniform_content` and `--scale_range` change nothing in Apple's own
output on any image tried, including one with a uniform black border and
one with a full alpha ramp, so what they are for is still to be found.

KTX, KTX2 and DDS are read as inputs.  `ktx_parse` had never read version
2's level index, so every version 2 container looked empty; it does now,
and the unpacker learnt halves, floats, version 2's tight rows and BGRA8's
reversed channels.  The DDS reader is the writer's header backwards.  A
compressed container is refused whichever of the three it is, as Apple's
tool refuses it -- ImageIO will decode some of them, and answering where
Apple says nothing is not the same tool.

The input's format is the default and `--compression_format` is the
answer: an RGBA8 container converts to RGBA8 unless asked for something
else, an image file to RGBA32.  Channels the target does not carry are
dropped and channels it gains are filled.  A compressed format is not
accepted here; Apple's segmentation faults on it.

The alpha mode applies on the way in, which is easy to miss -- Ignore is
the default, so a four channel container converts with ones in alpha
whatever it came in with, and because the Kaiser filter over a constant
one gives 1.0000002 rather than one, getting this wrong surfaces two
levels down and looks like a mip chain problem.  The alpha stays gone all
the way down: Apple write exactly one at every level, so it is thrown away
again after the chain and not only on the way in.

A container carries a chain and both modes carry it across.  Every level
the file holds is kept, whatever the format and whatever `--alpha_mode`
says, and the chain is only extended past them; compressing one encodes
each of those levels as it stands, so patching a level of an RGBA8
container and asking for BC1 changes that level's blocks and no others.

A flip, a gamma or a normal map does not change that -- each is applied
to every level the file brought.  `--max_extent` is the one option that
really does throw them away, because it resizes the base and the levels
behind it no longer line up: a five level sixteen square container cut to
eight comes out with four levels that owe nothing to the ones it had.
And `--max_mipmaps` cuts a chain that came in too long as readily as it
stops one being built.

That last sentence replaced a wrong one, and the way it was wrong is worth
keeping.  The experiment was to zero a level and ask whether the zeros
came back.  They did not, for the four channel formats under Ignore, so
the chain looked rebuilt there.  What actually came back was the zeroed
level with its alpha rewritten to one -- which under Ignore is exactly
what keeping it looks like, the RGB zero as written and only the alpha
moved.  Patch a level's colour and leave its alpha alone and the patched
level comes back out unchanged, for every uncompressed format and all
three alpha modes.  An experiment that disturbs the thing being measured
answers a different question.

That is also what the RGBA16 gap was.  Converting a half float container
rebuilt colour one unit in the last place of a half away from Apple's, in
the mip levels only, level zero identical -- because it rebuilt at all.
The source's levels were filtered from the image before it was quantised
to half, and filtering the quantised base again cannot land in the same
place.  Handed the same level zero as a one level file, the rebuild here
is byte for byte Apple's.

Alpha in the half float formats is its own quirk.  A channel the format
does not carry reads as zero and alpha reads as one, except at sixteen
bits, where Apple's fill is the float whose bits are the integer one
rather than the float one: 1.4e-45, the smallest denormal.  An R16
container to RGBA32 with `--alpha_mode=Preserve` writes 0x00000001 in
every alpha where R8 and R32 write 1.0, and `--alpha_mode=Premultiply`
multiplies the colour away to nothing.  The premultiply itself applies to
every level the input brought with it, not only the base, since each of
those levels has its own alpha; levels this tool filtered are left alone,
which is what an image file gets, being one level.

Version 1's padded rows are read as if they were tight, which is Apple's
bug and is reproduced deliberately: an R8 level two texels wide comes back
as its two bytes and then the two bytes of padding behind them.  Reading
the file correctly would put a different image through the rest of the
tool than their tool has.

1521 of 1521 converting -- thirteen uncompressed formats each way, three
images, three alpha modes -- against 106 of 1521 before this work, most
of that being `--compression_format` reaching nothing.  891 of 891
compressing a container to a block format, against 37 of 75.  2680 of
2680 over the options above against four output kinds.

Two output side bugs fell out of that sweep, neither of them about
containers.  A DDS with one level says dwCaps 0x00001000, plain texture,
where a chain says 0x00401008 -- the same distinction dwFlags already
makes a hundred bytes earlier, and the writer was making it in one place
and not the other.  And a decompressed DDS is always the four channel
spelling: BC4 comes out R8 in a KTX and DXGI 28 in a DDS, three times the
bytes, with the EAC decoder's invented alpha of one showing through as
the difference until it was made zero.

Two formats that had no decoder here now have one, and both are lessons
about which answer is the right one.

ETC2_RGB8A1 got a punchthrough decoder written from the specification --
all four modes, the `{ 0, +m, 0, -m }` modifier table the opaque bit
selects, selector two as a transparent texel.  It agreed with Apple byte
for byte on four images, and then the images turned out to hold no
punchthrough blocks at all: `--alpha_mode=Ignore` had forced alpha to one
before the encoder saw them.  Two images with hard zero-or-255 alpha
encoded under Preserve come out three quarters punchthrough, and there
the correct decoder disagrees everywhere.  Apple write alpha 255 across a
texture encoded with holes in it: they hand the block to the plain ETC2
decoder, which has no punchthrough and reads the bit that carries it as
ETC1's `diff`, so a block with the bit clear comes back decoded as an
individual mode block.  The answer was a table entry, and the decoder was
thrown away.

BC7 was said here to fail an assertion on any conforming block.  It does
not: NVTT reads seven of the eight modes correctly and byte for byte as
Apple do.  Only mode 0 dies, because `avpcl_mode0.cpp`'s `read_header`
has the line that consumes the mode bit commented out where every other
mode's begins with it, so its own bounds assertion fires and NVTT's
handler calls `exit(2)` -- silently.  BC7 is in the table now and a level
holding a mode 0 block is refused with a message instead; three of eight
test images have one.  Apple's answer for such a block is not a decode of
it: a reference mode 0 written from the specification reproduces none of
its sixteen pixels, nor does a model of NVTT's mis-parse, nor a dozen
variations around the two.  Whatever they run there is not this code
path and has not been identified.

Decompression is 215 of 220, against 180 before, the five being that one
image's BC7.

`--build_cubemap` takes six inputs into six faces, in both modes and all
four output formats.  The faces are six independent chains -- a face is
bit for bit what converting that image alone gives -- so the work is in
the containers: version 1 writes the level's imageSize once and then each
face behind it padded to four bytes, version 2 counts the whole level, and
the `.h` output names its arrays `Mip<n>Face<n>` and calls
`ATC_CreateTextureCube`, which takes one side rather than two.  DDS gets
none: Apple write none, say `Error: DDS file support only supports 2D
images` on stderr, and still exit zero.  Six inputs exactly: one, two,
five and seven are all refused.

`--build_mips` takes its inputs as the levels of one chain rather than as
one image: input zero is the base and each one after it is the level of
that index, kept verbatim, with the chain extended past the last of them
by filtering onward.  Only level zero of each input is taken, so a
container that carries a chain of its own contributes one level and not
its own.  Every option that touches a container's kept levels touches
these the same way -- a flip, a gamma, the alpha mode, the premultiply --
and `--max_mipmaps` truncates as it does anywhere.

Each level has to be the level it claims to be.  The dimensions are
checked against the base shifted right, with no clamp at one, so a sixteen
by eight base takes four levels and not five: level four would have to be
one by zero, and nothing is.  The format is checked too, and second: an
image file reads as RGBA32 and a container as whatever it says, so a PNG
handed in beside a KTX fails even when its size is right.  The check runs
after `--max_extent` has resized the base, which is Apple's order and is
visible in their message -- `--max_extent=8` with a sixteen and an eight
is an error, and with a sixteen and a four is not.

The combining modes are mutually exclusive, and the pair Apple name is the
first two present in the order array, cubemap, volume, mips, whichever way
round they were given.  Their arity and exclusivity complaints go to
stderr with the usage on stdout, which is the opposite way round from most
of this tool's messages, and `--build_cubemap` says "requires at size
input textures" -- their typo, kept, because the text is what a caller
matches on.

Conversion announces the combining modes by name: `Building cubemap
texture <output>`, `Building volume texture <output>`, `Building mip
mapped texture <output>`, where an ordinary conversion says `Converting
<input>`.  A chain says nothing at all until its levels have been checked.

`--build_array` takes its inputs as the elements of a 2D array, each one
an independent chain, bit for bit what converting that image alone gives
-- the same shape as a cubemap's faces, and the writers differ in one
place.  Version 1 counts `imageSize` per face for a cubemap and per level
for an array, all its elements together, and the header says
`numberOfArrayElements` where a cubemap says `numberOfFaces`; version 2
says `layerCount` against `faceCount`.  The `.h` output names its arrays
`Mip<n>Element<n>`, sets `numElements` to the count where everything else
sets one, and calls `ATC_CreateTexture2DArray`, which takes the width,
the height, the element count and then the level count.  DDS gets none.

Every element has to be the size of the first, since they are one texture
and not a collection.  At least two of them, and, like the other three,
`--build_array` is not recorded in TC_Options.

`--build_volume` stacks its inputs into one image with a depth, and the
chain halves that too, dropping an odd slice at the end.  The filter
across slices is not a three dimensional kernel -- which is what nvimage's
`downSample` would have given -- but the plain mean of two slices that
have each been through the two dimensional mip filter.  Both containers
carry the whole slab as one level, so only `pixelDepth` is new; the
encoders take a two dimensional image, so a compressed volume is encoded a
slice at a time and the slices laid end to end.  The `.h` output names its
arrays `Mip<n>Slice<n>` and calls `ATC_CreateTexture3D`, keeping the Slice
names once the levels are one slice deep, because the name follows the
texture and not the level.  Two slices at least, and DDS gets none.

Still to write, in the order they are worth doing:

`--gamut_in`/`--gamut_out` beyond the `.h` output; the EXR and HDR inputs
Apple's usage also lists; and BC7 mode 0.

`--max_extent` announces itself: `Resized image to (width: %d, height:
%d, depth: %d)`, once for every image it resized -- a face each for a
cubemap, a slice each for a volume -- and the conversion path says it
twice over, once before the banner and once after.  That is two passes
showing through: Apple load and resize in one, then do the work in
another, which is also why a `--build_mips` chain whose levels do not
line up prints the first of these and no banner at all.  This tool
printed none of it, and the count is what gave away the next one.

A volume is resized a slice at a time and keeps its slice count.  Here
`--max_extent` was halving the depth along with the width and the height,
because it went through the mip filter, which pairs slices -- so a two
slice volume cut to eight came out one slice deep where Apple keep both.
Their message says `depth: 1` each time, being the depth of the image
being resized rather than of the volume it goes into, which is the same
thing said out loud.

A PNG whose every pixel is transparent came back from ImageIO with its
colour zeroed, where Apple keep it: a one by one image of (244, 131, 157,
0) converts to (244, 131, 157, 255) there and did to (0, 0, 0, 255) here
under the default alpha mode.  An image with one opaque pixel in it comes
back whole, which is why this hid for so long, and every option
`CGImageSourceCreateImageAtIndex` takes was tried -- none of them keeps
the colour, so Apple are not reading PNGs this way.  NVTT is linked here
already, for its filters and its encoders, and the stb_image it bundles
reads the file as it is written; so an image that is transparent all the
way across is read a second time through `nv::ImageIO::load` and its
colour taken from there.  If the file really is black behind its
transparency the second read says so too.

Error text and streams are a third: Apple put the specific complaint on
stdout for some failures and stderr for others, and follow a failed
compression with `Error: Failed to compress texture` and a failed
conversion with `Error: File Not Found!` whatever went wrong.  The
combining modes match them exactly now; the rest of the messages do not.

Accepted and inert in Apple's tool as well, so nothing is owed until a
case is found where they bite: `--crop_uniform_content`, `--scale_range`,
`--resize_filter` (even with a `--max_extent` that resizes),
`--resize_round_mode` (which rejects every value tried, its vocabulary
unknown), `--flip_z` (even on a volume), `--metrics`, `--decompressor`
and `--decompression_format`.

Five measurements settled the pixel path, and none was guessable:

  - **The mip chains are NVTT's.**  An impulse through Apple's Kaiser
    gives weights of 0.006729, 0.013252, -0.033884, -0.054839, 0.139929
    and 0.428814, and NVTT's PolyphaseKernel over a KaiserFilter of width
    3 and alpha 4, box-sampled 32 times per tap, gives exactly those.
  - **Eight-bit samples are multiplied by 1/255, not divided by 255.**
    The reciprocal is inexact in binary and carries its rounding through:
    96 comes out 0x3ec0c0c2 that way and 0x3ec0c0c1 by division.
  - **Samples are read straight, not premultiplied.**  A bitmap context
    can only be asked for premultiplied alpha and dividing it back out
    does not recover the original -- a pixel stored (237, 191, 136, 70)
    returns (236, 189, 134) where Apple write 237, 191, 136.  The image's
    own data provider hands over what was decoded.
  - **`--alpha_mode=Premultiply` folds alpha into the base level only.**
    The mip chain is built from the straight colour: Apple's Premultiply
    and Preserve write byte-for-byte the same second level.
  - **An EAC sample is narrowed to eight bits through sixteen.**  The
    eleven bits are replicated up into a sixteen bit channel, which maps
    2047 onto 65535 rather than onto 65504, and that is divided by 257.
    Nothing in one step matches: over the 256 distinct values the four EAC
    files decode to, truncation is one too low for 1172 and rounding is
    wrong for 108 of them.

The four compression qualities are astcenc's own presets, measured by
compressing the same image both ways: Fastest is FASTEST, Normal FAST,
Production MEDIUM, and Highest the EXHAUSTIVE search rather than the
very-thorough one below it.  NVTT's quality enum is Apple's four names in
Apple's order, so BC passes straight through; etc2comp takes a number from
0 to 100 instead and the four land on 0, 40, 80 and 100, with REC709 as the
error metric for all five of its formats.  stb spends its extra refinement
pass only at Highest.  `--channel_weighting` decides
`ASTCENC_FLG_USE_PERCEPTUAL` and is Perceptual by default; `--alpha_weight`
adds `ASTCENC_FLG_USE_ALPHA_WEIGHT`.

Two keys are format description rather than annotation, so
`--disable_annotation` leaves them: `KTXmetalPixelFormat`, which only the
ASTC formats carry and whose Metal enumerants are not contiguous (209 is
unused, so the column is measured rather than counted), and
`com.apple.image.premultipliedAlpha`, written only when the colour actually
was premultiplied.

**Residual.**  Roughly one partial edge block in a hundred differs.  It is
not a settings difference -- quality was swept 0 to 100 across both
profiles and every flag combination, and 14 of 15 blocks is the ceiling --
and not a version difference: astcenc 4.8.0 and 5.7.0 agree with each other
and differ from Apple in exactly the same blocks.  Padding the image to the
block grid by clamp, zero or mirror does not reproduce it either.  Whole
blocks and every level of a power-of-two image match; only blocks the image
does not fill are affected.

Still to write: the other three encoder families behind `--mode=compress`
(the ports are in; the wiring is not), decompress and compare, the KTX2 and
DDS writers, resizing, and the gamma and gamut handling.  One more
residual: the Box mipmap filter's final 1x1 level differs by a unit in the
last place, a summation-order difference in a non-default filter.

#### Why xcsigningtool is not reimplemented

It has one subcommand, cloud-sign, and it does not sign anything locally.
The certificate is a managed one held in an account on developer.apple.com
and the private key never leaves Apple; the tool authenticates with an App
Store Connect key and asks their service to sign.  DVTPortal, which it links
for this, is a client for that service, and builds its endpoints at runtime
rather than carrying them as strings.

So there is no file format to read instead of the framework, as there was for
xcresulttool and xccov, and nothing that could be checked against Apple's
without a developer account and their servers answering.  Local signing --
ad-hoc, from a keychain identity, or from a .p12 -- is what our own codesign
already does.
| `xctest` | missing — XCTest bundle runner |
| `xcdebug` | missing |
| `xcindex-test` | not attempted, deliberately — see below |

#### Why xcindex-test is not reimplemented

It is a diagnostic REPL over the build-system APIs the index service calls,
and Apple's own help says it may be renamed or removed.  Eleven of its twelve
actions -- create-build-description, print-index-build-settings, index-files,
prepare and the rest -- drive Xcode's build service live; there is no file
format standing behind them the way the .xcresult store stands behind
xcresulttool, so there is nothing to read instead of the frameworks.  It
links DVTFoundation and IDEFoundation for workspace loading, and its output
carries per-run timings, so most of it cannot be compared byte for byte
either.

The one action that does not need any of that, list-schemes, reads .xcscheme
files -- which `xcodebuild -list` already does here, in
src/openxc-tools/xcodebuild/project.c.
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
links against it, and clang's resource directory (`lib/clang/<ver>`) via
`P_TREES`. That last one is not optional: without clang's own `stdarg.h` and
friends, anything past trivial C fails with `'stdarg.h' file not found` raised
from inside the SDK's headers, which reads like an SDK problem and is not. **`ports` runs before `progs`** in the top-level `all` for
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


### Carried but not built, and why

Three submodules under `src/extras` are in the tree and deliberately not
wired. Each was tried; these are the blockers, so nobody has to find them
twice.

- **elfsec** — the ELF counterpart to `machsec`, by the same author. It
  includes `<libelf.h>`, `<gelf.h>` and `<elf.h>`, none of which macOS has.
  The candidates each fail for their own reason: elfutils' libelf is
  LGPL/GPL, Michael Riepe's is LGPL, and this tree is BSD-3-Clause;
  elftoolchain's is BSD, but its `elf.h` is FreeBSD's and elfsec is written
  against Linux's constants. That is a port of libelf, not a wiring job, and
  it buys one ELF tool on a macOS-only tree — `patchelf` is already here and
  parses ELF itself. Capstone, the other half of what it needs, *is* now a
  port, so if libelf ever lands this is a small entry away.

- **MTool** — a Mach-O and dyld-shared-cache analyser, an Xcode project.
  Two things stop it. Its `mtool` target includes `<mach-o/dyld_cache_format.h>`
  and `<mach-o/dyld_process_info.h>`, which come from Apple's *internal* SDK;
  our own `src/apple/dyld/include` has both, but they use
  `__API_UNAVAILABLE(bridgeos)`, and `bridgeos` is not a platform the public
  `Availability.h` knows, so the header will not even parse. Beyond that its
  documented first step is `sh ref/clone.sh`, which clones six Apple projects
  and llvm-project *into the submodule* and then runs an
  `extract_external_headers` target that writes into `apple_headers/`. Nothing
  here writes to a submodule. Wiring it means building against the internal
  SDK this tree reconstructs (`mk/sdk-headers.mk`), and pointing
  `HEADER_SEARCH_PATHS` there rather than at `ref/`.

- **mootool** — a Ruby tool for Mach-O, IPAs and Apple data formats. Its
  gemspec depends on `activemodel`, `apple-data`, `CFPropertyList`, `ecies`,
  `gtk3`, `lzfse`, `lzss` and a dozen more. Every one would have to become a
  vendored gem, one of them binds GTK3, and a build that fetches from
  rubygems.org is not a build that runs twice and gets the same answer. This
  needs a Ruby story for the whole tree before it needs a port fragment.

Two outside projects were looked at and are not being taken up:

- **gollvm** (`go.googlesource.com/gollvm`) — an LLVM-based Go compiler, and
  we ship both Go and LLVM, so it looks like a fit. Its own README says
  "currently supported only for x86_64 and aarch64 Linux". There is no Darwin
  support to enable: it would need a Mach-O TLS story in the driver and a
  darwin syscall layer in libgo. It also wants gofrontend, libffi and
  libbacktrace checked out inside `llvm-project/llvm/tools`, which is not how
  submodules are laid out here, and gofrontend is GPL-3 — the same objection
  that ruled out libdwarf for `atos`, only stronger.

- **macos-minimal-sdk** (`tinygo-org/macos-minimal-sdk`) — not adopted, but
  worth having read. It exists to cross-compile to macOS *from Linux*, which
  is out of scope here. Its headers are macOS 11.5 and C/POSIX only, with no
  frameworks, where ours are macOS 26.5 and include CoreFoundation, Security,
  IOKit, WebKit, Foundation and Kernel. Its link stubs are a `libSystem.s` of
  symbol names harvested by parsing headers with clang; ours are real `.tbd`
  files generated from the host's own dylibs (`mk/scripts/make-tbd.sh`),
  which is more faithful and is something only a macOS host can do — exactly
  the assumption this tree may make and that one may not.

  The two hardest things its `update.sh` does, we already do: stripping the
  `//Begin-Libc` / `//End-Libc` blocks out of Libc's headers
  (`mk/scripts/strip-libc-private.sh`), and running xnu's
  `make_symbol_aliasing.sh` and `make_posix_availability.sh` to generate
  `sys/_symbol_aliasing.h` and `sys/_posix_availability.h`
  (`mk/sdk-headers.mk`, plus our own `mk/scripts/availability.pl`).

  Two things it does that we do not, both recorded here rather than adopted:

  - It rewrites `__API_AVAILABLE` and `__API_UNAVAILABLE` in `Availability.h`
    into variadic no-ops. That is a blunt instrument, and our availability
    story is already delicate — `mk/sdk-headers.mk` explains at length why
    xnu's `EXTERNAL_HEADERS` set and darwin-xnu-build's fakeroot set are not
    interchangeable. But it is the same wall MTool hits above:
    `__API_UNAVAILABLE(bridgeos)` will not parse against the public
    `Availability.h`, because `bridgeos` is not a platform it knows. If that
    ever needs solving, this is the cheap version of the answer.
  - It replaces the headers whose Apple copyright carries no licence grant
    with public-domain rewrites. See the note below.

### Headers with no per-file licence grant

A sweep of the 2,661 headers this tree installs into `MacOSX.sdk/usr/include`
finds 14 that say only "Copyright (c) … Apple Inc. All rights reserved." with
no grant of any kind in the file:

| Path | Comes from |
|------|------------|
| `stdint.h` | `src/apple/libc/include` (and xnu's `EXTERNAL_HEADERS`, identical) |
| `removefile.h` | `src/apple/removefile` |
| `arm/_limits.h`, `arm/_param.h`, `arm/_types.h`, `arm/cpu_x86_64_capabilities.h`, `arm/disklabel.h`, `arm/profile.h`, `arm/psl.h`, `arm/reg.h`, `arm/signal.h`, `arm/vmparam.h` | `src/apple/xnu/bsd/arm` |
| `objc/NSObjCRuntime.h`, `objc/NSObject.h` | `src/apple/objc4` |

Every other header carries APSL 2.0, Apache 2.0, CDDL, a BSD notice, or
`@APPLE_LLVM_LICENSE_HEADER@`. The 14 are not unlicensed code: each comes
from a submodule whose *distribution* is APSL 2.0, and the omission is a
missing per-file notice rather than a missing grant. So this is a note, not
a blocker.

It is worth knowing because macos-minimal-sdk hit the same list and took the
cautious route — its author rewrote them from scratch and put the rewrites in
the public domain, which is why that repo can claim to be open source end to
end. If this tree ever wants the same claim, the work is small and mechanical:
these headers hold ABI constants and standard C typedefs, not expression.
`stdint.h` is ISO C99's table, `arm/_limits.h` is a single `#define`, and the
`objc/` two are the runtime's public declarations.


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
bmake -f mk/tool.mk TOP=$PWD T_DIR=openxc-tools/codesign T_PROG=codesign T_BIN=usr/bin
```

### Use the binaries this tree builds

When a task here needs one of the tools this tree builds, run the built copy
under `build/release/` by path.  A bare name goes through `PATH` and picks up
Homebrew's or the system's copy of the same tool instead, which quietly
answers questions about the wrong artifact.

This matters more than it looks like, because the tree builds more than the
Xcode tools themselves: `ipsw` and the Go that builds it, `git`, `perl`,
`python3`, `pip3`, `bmake`, `xmllint`, `xsltproc`.  Check `mk/ports.mk` and
`build/release/usr/{bin,local/bin}` before reaching for a tool, and before
proposing to add one that may already be there.

```sh
IPSW=$PWD/build/release/usr/local/bin/ipsw
"$IPSW" dyld info --dylibs "$DSC"
```

The general-purpose ipsw skill says to install it from Homebrew; that is right
everywhere except in this tree, where `mk/port.d/ipsw.mk` already builds it
from `src/extras/ipsw` with our own Go.

### Testing

For codesign specifically (our most tested tool):

```sh
cc test.c -o test_bin
build/release/usr/bin/codesign -f -s - test_bin
codesign --verify --strict test_bin
```

### Keeping README.md current

**Every commit that adds a tool, a port, or a user-visible feature updates
`README.md` in the same commit.** Not afterwards, not in a batch later.

`README.md` is the only file that answers "what does this project build today",
and it is worthless the moment it stops being true. The specific things that go
stale fastest:

- the program and port counts, and the table of what lands where
- the "What is missing" list, when something stops missing
- the build instructions, when a target or tier knob changes

`bmake list-progs` and `bmake list-ports` print the current inventory, and
`bmake check` will tell you whether the tree actually contains what the README
claims.

### Adding a New Tool

1. Put the sources somewhere under `src/`. For our own reimplementations that
   is `src/openxc-tools/<tool-name>/`; for an imported component it is wherever the
   submodule already keeps them — do not move or copy them.
2. Add one line to `mk/progs.mk`:
   ```
   PROGS+=	openxc-tools/<tool-name> <tool-name> usr/bin
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
1. Our reimplemented tools (in `src/openxc-tools/`) are BSD-3-Clause
2. Each submodule retains its own license
3. No proprietary Apple code (except what's in open-source submodules)
4. No copying of Apple's closed-source binaries
5. All third-party dependencies must have compatible licenses
6. Build system must be reproducible from source alone

Fourteen of the SDK headers carry an Apple copyright with no per-file licence
grant. They are covered by their submodules' APSL 2.0 distributions and are
named under "Headers with no per-file licence grant" in Stage 5 above.

---

## 13. Key Files to Know

| File | Purpose |
|------|---------|
| `docs/DOCUMENTATION.md` | Full audit of our tools vs. Apple's |
| `docs/CLAUDE.md` | This file — development instructions |
| `src/openxc-tools/codesign/cs_sign.c` | Code signing entry point (most tested) |
| `src/openxc-tools/codesign/cs_macho.c` | Mach-O parsing and __LINKEDIT updates |
| `src/openxc-tools/codesign/cs_blob.c` | CodeDirectory/SuperBlob construction |
| `src/openxc-tools/xcodebuild/xcodebuild.c` | Main build orchestration logic |
| `src/openxc-tools/xcodebuild/project.c` | .pbxproj parser |
| `src/openxc-tools/xcodebuild/plist.c` | Property list parser |
| `src/openxc-tools/xcrun/xcrun.c` | Tool location and execution |
| `Makefile` | Top-level bmake build |
| `mk/progs.mk` | The program inventory — add a tool here |
| `mk/tool.mk` | The per-program build engine |
| `mk/xcodetools.sys.mk` | Global flags and tier gating |
| `mk/bundle.mk` | Emits the `.xctoolchain` / `.sdk` bundle metadata |
| `mk/port.mk` | Driver for components with their own build system |
| `mk/ports.mk` | The port inventory |
| `src/openxc-tools/common/devpath.c` | Finds our Developer directory at runtime |
| `configs/xcrun.ini` | Default SDK/toolchain config |

---

## 14. Quick Start for New Contributors

```sh
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/xnuports/xcode-tools.git
cd xcode-tools

# 2. Initialize nested submodules (distribution-Developer_Tools has sub-submodules)
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
