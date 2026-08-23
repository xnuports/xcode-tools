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
4. **Build hygiene:** All build artifacts go to `build/`
   (`build/usr/bin/` for binaries, `build/obj/<tool>/` for objects).
   Source tree must remain pristine.
5. **Submodule discipline:** Each external dependency is a git submodule.
   Nested submodules must be populated with `git submodule update --init --recursive`.

---

## 3. Current Repository Structure

```
xcode-tools/
├── Makefile                        # Top-level bmake build system
├── .gitmodules                     # 10 submodule definitions
├── .gitignore
├── LICENSE.BSD-3
├── README.md
├── docs/
│   ├── DOCUMENTATION.md            # Comprehensive audit vs. Xcode Developer
│   └── CLAUDE.md                   # This file
├── configs/                        # SDK/toolchain INI configuration
├── scripts/                        # Toolchain shim scripts (cc.sh, clang.sh, etc.)
├── xctoolchain/                    # Submodule: Xcode toolchain configurations
├── include/
│   └── ld-internals/               # Submodule: ld64 private headers
├── src/
│   ├── Makefile                    # Delegates to xcode/
│   ├── Makefile                    # Top-level src delegator
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
│   └── xcode/                      # OUR reimplemented Xcode tools (10 tools)
│       ├── Makefile
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
    ├── obj/
    └── usr/bin/
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
| `python-apple-support` | `src/python-apple-support/` | heads/main | Python XCFramework build system |
| `PlistBuddy` | `src/PlistBuddy/` | heads/main | PlistBuddy |
| `pngcrush` | `src/pngcrush/` | v1.8.1 | pngcrush |
| `xctoolchain` | `xctoolchain/` | heads/master | Xcode toolchain configs (.xcconfig files) |
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
| xml2man | Docs | ❌ No source (Apple proprietary) |
| agent, ba-package, ba-serve | Build Assistant | ❌ No source (Apple internal) |
| backgroundassets-debug | Debug | ❌ No source (Apple internal) |
| compositeMD5 | Archive | ❌ No source (Apple internal) |
| convertRichTextToAscii | Conversion | ❌ No source (Apple internal) |
| filtercalltree | Debug | ❌ No source (Apple internal) |
| resolveLinks | File | ❌ No source (Apple internal) |
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

**Hierarchy:**
```
Makefile (top-level)
  → src/Makefile
    → src/xcode/Makefile
      → src/xcode/<tool>/Makefile (x10)
```

**Key design decisions for bmake compatibility:**
- `:=` assignment (not `?=` for CC, since bmake's default CC is `cc ${PIPE}`)
- `${.CURDIR}` instead of `$(CURDIR)` for directory paths
- `.for` loops to generate explicit compile rules (bmake doesn't support path-prefixed `%.o: %.c` pattern rules)
- `!=` operator instead of `$(shell ...)` for command substitution
- `$>` (not `$<`) if automatic variables for first-prerequisite are needed in explicit rules
- All build output to `build/` directory

**Build commands:**
```sh
bmake          # Build all 10 tools
bmake clean    # Remove build/ directory
bmake install  # Install to PREFIX (default: /opt/xnuports/opt/xcode-tools)
```

### 8.2 Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `BUILD_DIR` | `${.CURDIR}/build` | Build output directory (absolute) |
| `PREFIX` | `/opt/xnuports/opt/xcode-tools` | Installation prefix |
| `CC` | `clang` | C compiler (use `:=` not `?=` in top-level) |
| `DESTDIR` | (empty) | Staged install root |

---

## 9. Phased Roadmap

### Phase 1: Build & Integrate Existing Sources (Current Priority)

Goal: Get all submodule sources to compile and produce working binaries.

**Tasks:**

1. **cctools integration**
   - Create Makefiles for each tool in `src/dist-dev-tools/cctools/misc/`
   - Build: ar, nm, lipo, strip, strings, size, otool, vtool, segedit, install_name_tool, bitcode_strip, codesign_allocate, ctf_insert, libtool (Apple variant)
   - Integrate into top-level Makefile hierarchy
   - Output to `build/usr/bin/`

2. **ld64 integration**
   - Build `ld` and `ld-classic` from `src/dist-dev-tools/ld64/`
   - Requires cctools headers as build dependency

3. **developer_cmds integration**
   - Build: asa, ctags, indent, lorder, rpcgen, unifdef
   - Each has a `Makefile` or `plist` in its directory

4. **headerdoc integration**
   - Build: headerdoc2html, hdxml2manxml
   - Note: headerdoc is Perl-based (uses `gatherHeaderDoc.pl`)

5. **dist-dev-tools remaining**
   - Build: bison, flex, gnumake, gperf, gm4
   - Integrate tapi (text-based API tool)
   - CoreOSMakefiles and pb_makefiles are infrastructure, not standalone tools

6. **Git integration**
   - Build git v2.55.0 from `src/git/`
   - Output: git, git-receive-pack, git-shell, git-upload-archive, git-upload-pack
   - Requires: openssl, curl, expat, pcre2, etc.

7. **CPython + python-apple-support integration**
   - Build CPython 3.14.7 using python-apple-support build system
   - Output: python3, pip3, pydoc3, 2to3
   - python-apple-support creates XCFrameworks — may need different install path

8. **pngcrush integration**
   - Build pngcrush v1.8.1 from `src/pngcrush/`
   - Uses bundled libpng/zlib — verify licensing

9. **PlistBuddy integration**
   - Already has source in `src/PlistBuddy/`
   - Build: PlistBuddy

10. **Verify toolchain compatibility**
    - Test that our clang (from llvm-project) can compile our cctools
    - Test cross-compilation toolchain (configure scripts, etc.)
    - Verify bmake build of all tools

**Deliverable:** A complete set of command-line tools in `build/usr/bin/` that
match Apple's toolset (minus the Apple-proprietary tools).

### Phase 2: Missing Xcode Tools (Our Reimplementations)

Goal: Replace or supplement our 10 tools with more capable versions, or
implement new tools that Apple hasn't open-sourced.

**Tasks:**

1. **codesign** — Add CMS-based detached signatures:
   - Support `codesign -s "Developer ID: ..."` (certificate-based signing)
   - Implement CMS blob generation (using OpenSSL)
   - Support `--timestamp` (notarization workflow integration)
   - Support `--deep` recursive signing
   - Support `--options=runtime` (hardened runtime)
   - Support `--preserve-metadata` for re-signing with preserved entitlements

2. **devicectl** — Expand device management:
   - Add app installation/removal (`devicectl device.app.install`)
   - Add screen recording/streaming
   - Add file push/pull (`devicectl device.file`)
   - Add log streaming (`devicectl device.log`)
   - Add process management (`devicectl device.process`)

3. **notarytool** — Full API coverage:
   - Add `notarytool history`
   - Add `notarytool log`
   - Add `notarytool cancel`
   - Add `--key-path`/`--key-id`/`--issuer` CLI options
   - Integrate stapling after successful notarization

4. **xctrace** — Real tracing backend:
   - Integrate with DFR (Device File Relay) or implement via `xctrace record`
   - Add `xctrace list` (template listing)
   - Add `--template`, `--device`, `--time-limit` options
   - Add `xctrace export` with full format support

5. **xcodebuild** — Actual compilation support:
   - Implement per-file compilation using our clang toolchain
   - Add proper Xcode project build phase execution
   - Add `-destination` / `-scheme` / `-configuration` support
   - Add DerivedData management
   - Add XCTest integration

### Phase 3: Apple-Proprietary Tools (Reverse Engineering / Reimplement)

Goal: Implement tools that Apple has not open-sourced.

**Priority order (by dependency chain):**

1. **actool** — Asset catalog compiler (critical for iOS/macOS apps)
   - Parse `.xcassets` directories
   - Generate compiled asset catalogs (CAR files)
   - Support AppIcon, Complications, Stickers, etc.
   - Requires understanding of Apple's binary asset catalog format

2. **momc** — Core Data model compiler
   - Parse `.xcdatamodeld` directories
   - Generate `.momd` bundles
   - Requires understanding of Core Data binary format

3. **coremlc** — Core ML model compiler
   - Parse `.mlmodel` files
   - Generate compiled model bundles
   - Requires understanding of Core ML compilation pipeline

4. **ibtool** — Interface Builder compilation
   - Parse `.xib`/`.nib`/`.storyboard` files
   - Generate compiled nib files
   - Requires AppKit runtime headers (private)

5. **stapler** — Signature stapling
   - Add App Store receipt + notarization ticket to archives
   - Requires understanding of CMS ticket format

6. **xcsigningtool** — Signing identity management
   - List and manage code signing identities
   - Keychain integration

7. **xctest** — Unit test runner
   - Execute XCTest test bundles
   - Report results in XCTest format

8. **xccov** — Coverage reporting
   - Parse LLVM coverage data
   - Generate reports (text, JSON, HTML)

9. **xcresulttool** — Test result processing
   - Bundle and unbundle test results
   - Export attachments

10. **agvtool** — Version string management
    - Increment/decrement version numbers
    - Read/write version info in bundles

11. **altool/iTMSTransporter** — App Store delivery
    - Validate and upload archives to App Store
    - Requires App Store Connect API integration

### Phase 4: SDK & Platform Infrastructure

Goal: Create our own SDK bundle to match Apple's Developer directory.

**Tasks:**

1. **SDK directory structure**
   - Create `Platforms/MacOSX.platform/` (and other platforms)
   - Create `SDKs/MacOSX.sdk/` (and other SDKs)
   - Populate with headers and libraries

2. **SDKSettings.plist generation**
   - Convert INI configs to Apple's property list format
   - Or keep INI format and have xcrun/xcodebuild convert

3. **Headers**
   - Copy system headers from macOS SDK
   - Include toolchain headers (clang, Swift)
   - Include private framework headers (where open-sourced)

4. **Libraries**
   - dylib/lib files for SDK linking
   - Swift runtime libraries
   - Objective-C runtime (from objc4)
   - C++ standard library (from libcxx)

5. **Toolchain structure**
   - Build `Toolchains/XcodeDefault.xctoolchain/` layout
   - Populate with clang, swiftc, and all toolchain tools
   - Create toolchain info.plist

6. **Developer directory layout**
   - `Applications/Simulator.app` (or reference system's)
   - `Library/Frameworks/` (Python3, XcodeKit — may need Apple's)
   - `Makefiles/` (from CoreOSMakefiles)
   - `Tools/` (resource fork tools — currently binary-only)
   - `usr/bin/` (all our tools)
   - `usr/lib/` (libraries)
   - `usr/share/` (documentation, man pages)

---

## 10. Development Workflow

### Building

```sh
# Build all tools
bmake

# Build a specific tool
bmake -C src/xcode/<tool-name>

# Clean everything
bmake clean

# Install (to default PREFIX or custom)
bmake install
bmake install PREFIX=/usr/local
```

### Testing

For codesign specifically (our most tested tool):
```sh
# Sign a test binary and verify
clang test.c -o test_bin
bmake -C src/xcode/codesign build  # or use codesign from build/usr/bin/

# Verify the signed binary
codesign --verify --strict build/usr/bin/test_bin
spctl --assess build/usr/bin/test_bin
```

### Adding a New Tool

1. Create `src/xcode/<tool-name>/` directory
2. Add source files (`.c` and `.h`)
3. Create `Makefile` following the template:
   ```makefile
   PROG := <tool-name>
   OBJDIR := $(BUILD_DIR)/obj/$(PROG)
   BINDIR := $(BUILD_DIR)/usr/bin
   CC ?= cc
   CFLAGS := -Wall -Werror -O2
   SRC := <source files>
   .for src in $(SRC)
   $(OBJDIR)/$(src:R).o: $(src)
       mkdir -p $(OBJDIR)
       $(CC) -x c $(CFLAGS) -c $(src) -o $@
   .endfor
   all: $(BINDIR)/$(PROG)
   $(BINDIR)/$(PROG): $(OBJ)
       mkdir -p $(BINDIR)
       $(CC) $(OBJS) -o $@ $(LFLAGS)
   clean:
       rm -rf $(OBJDIR) $(BINDIR)/$(PROG)
   install: all
       mkdir -p $(DESTDIR)$(PREFIX)/usr/bin
       install -m 755 $(BINDIR)/$(PROG) $(DESTDIR)$(PREFIX)/usr/bin/$(PROG)
   ```
4. Add tool name to `src/xcode/Makefile` SUBDIRS list
5. Test: `bmake && build/usr/bin/<tool-name> --help`

### bmake Gotchas

| GNU Make | bmake Equivalent | Notes |
|----------|------------------|-------|
| `$(CURDIR)` | `${.CURDIR}` | Returns absolute path in bmake |
| `$(shell cmd)` | `VAR != cmd` | Use `!=` for command substitution |
| `$<` | `$>` or `$(src)` | First prerequisite (empty in bmake explicit rules) |
| `%.o: %.c` | `.for` loop | bmake doesn't support path-prefixed patterns |
| `?=` for CC | `:=` | bmake's default CC is `cc ${PIPE}` |

---

## 11. Testing Requirements

Every tool must pass:
1. **Build test:** `bmake` succeeds without errors or warnings
2. **Binary verification:** All binaries are valid Mach-O (use `file`)
3. **Functionality test:** Tools produce expected output for basic operations
4. **Clean test:** `bmake clean` removes all build artifacts
5. **Idempotency:** Re-running `bmake` after `bmake clean` produces same binaries

For specific tools:
- **codesign:** Must pass `codesign --verify --strict` and `spctl --assess`
- **xcrun:** Must find and execute tools in the Developer directory
- **xcodebuild:** Must parse `.pbxproj` and `.xcconfig` files correctly
- **pkgbuild:** Must produce valid `.pkg` archives (test with `pkgutil`)
- **productbuild:** Must produce valid distribution packages
- **simctl:** Must list available simulators (requires Xcode Simulator installed)

---

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
| `src/xcode/Makefile` | Tool iteration |
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
ls build/usr/bin/

# 5. Clean up
bmake clean
```

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
