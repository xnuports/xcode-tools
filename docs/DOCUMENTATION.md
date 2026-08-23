# xcode-tools: Comprehensive Audit & Documentation

This document provides a comprehensive audit of the `xcode-tools` project, comparing
our open-source reimplementation against Apple's proprietary Xcode Developer tools.

## 1. Repository Structure Overview

### Our Repository Layout

```
xcode-tools/
├── Makefile                          # Top-level bmake build system
├── .gitmodules                       # Submodule definitions
├── .gitignore
├── LICENSE.BSD-3
├── README.md
├── configs/                          # SDK/toolchain INI configuration
├── scripts/                          # Toolchain shim scripts (cc.sh, clang.sh, etc.)
├── xctoolchain/                      # Submodule: Xcode toolchain configuration
├── src/
│   ├── Makefile                      # Delegates to xcode/
│   ├── PlistBuddy/                   # Submodule: open-source PlistBuddy
│   ├── python-apple-support/         # Submodule: Python build system for Apple platforms
│   ├── cpython/                      # Submodule: CPython 3.14.7 source
│   ├── git/                          # Submodule: Git v2.55.0 source
│   ├── dist-dev-tools/               # Submodule: Apple open-source dev tools
│   │   ├── bison/                    # GNU Bison
│   │   ├── cctools/                  # Apple cctools (includes ld64)
│   │   ├── developer_cmds/           # Apple developer commands
│   │   ├── flex/                     # Fast Lexical Analyzer
│   │   ├── gnumake/                  # GNU Make
│   │   ├── gperf/                    # Perfect hash function generator
│   │   ├── gm4/                      # GNU M4
│   │   ├── headerdoc/                # Header documentation tool
│   │   ├── ld64/                     # Apple linker
│   │   ├── libgit2/                  # Git library
│   │   └── tapi/                     # Text-based API tool
│   ├── objc4/                        # Submodule: Objective-C runtime
│   ├── pngcrush/                     # Submodule: PNG optimization tool
│   ├── llvm-project/                 # Submodule: LLVM/Clang/LLVM project
│   ├── swift/                        # Submodule: Swift compiler
│   └── xcode/                        # Our reimplemented Xcode tools
│       ├── Makefile                  # Iterates over tools
│       ├── codesign/                 # Code signing & verification
│       ├── devicectl/                # Device management
│       ├── notarytool/               # App notarization client
│       ├── pkgbuild/                 # Package building (xar, BOM, payload)
│       ├── productbuild/             # Product building (distribution)
│       ├── simctl/                   # Simulator control
│       ├── xcode-select/             # Developer dir selection
│       ├── xcodebuild/               # Build orchestration
│       ├── xcrun/                    # Tool locator & executor
│       └── xctrace/                  # Trace recording & export
└── build/                            # Generated: build output
    ├── obj/
    └── usr/bin/
```

### Apple's Xcode Developer Directory Structure

```
/Applications/Xcode.app/Contents/Developer/
├── Applications/
│   └── Simulator.app                  # Simulator GUI application
├── Developer/
│   └── (macOS-specific development content)
├── Library/
│   ├── Frameworks/
│   │   ├── Python3.framework          # Python 3
│   │   └── XcodeKit.framework         # Xcode extension API
├── Makefiles/
│   ├── Carbon/
│   ├── CoreOS/
│   ├── pb_makefiles/
│   └── VersioningSystems/
├── Platforms/                         # 10 platform directories
│   ├── AppleTVOS.platform/
│   ├── AppleTVSimulator.platform/
│   ├── DriverKit.platform/
│   ├── iPhoneOS.platform/
│   ├── iPhoneSimulator.platform/
│   ├── MacOSX.platform/
│   ├── WatchOS.platform/
│   ├── WatchSimulator.platform/
│   ├── XROS.platform/
│   └── XRSimulator.platform/
├── Toolchains/
│   └── XcodeDefault.xctoolchain/
│       └── usr/bin/                  # 119 binaries (clang, swiftc, etc.)
├── Tools/                             # 6 legacy tools (Rez, DeRez, etc.)
│   ├── DeRez
│   ├── GetFileInfo
│   ├── ResMerger
│   ├── Rez
│   ├── SetFile
│   └── SplitForks
├── usr/
│   ├── bin/                          # 99 tools (devicectl, xcodebuild, etc.)
│   ├── include/                      # Headers
│   ├── lib/                          # Libraries (libclang.dylib, etc.)
│   └── share/                        # Documentation, bash-completion, etc.
└── config/                           # Internal config
```

## 2. Implemented Tools

We currently have **10 open-source reimaginations** of Apple's command-line tools:

### 2.1 codesign

**Source:** `src/xcode/codesign/` (7 files: codesign.c, codesign.h, cs_blob.c, cs_file.c, cs_macho.c, cs_sign.c, cs_verify.c)

**Capabilities:**
- Ad-hoc code signing (`codesign -s -`)
- Code signature verification (`codesign --verify`)
- Strict verification support (`--strict` flag)
- Mach-O binary parsing and manipulation
- CodeDirectory blob construction
- SuperBlob assembly (CS_SuperBlob format)
- Special slot computation (entitlements, requirements, Info.plist)
- Page hash computation
- Entitlements blob (DER and XML formats)
- Requirements blob generation
- `--linker` output support
- Force (`-f`) flag support
- `--preserve-metadata` for re-signing

**What works:**
- Ad-hoc signing passes `codesign --verify`, `codesign --verify --strict`, and `spctl --assess`
- Compatible with `bmake` build system
- OpenSSL integration for crypto operations

**What's missing compared to Apple's codesign:**
- No CMS-based detached signatures (only ad-hoc `-s -`)
- No certificate chain validation (no `-s "Developer ID: ..."` support)
- No hardened runtime flags (e.g., `--options=runtime`)
- No `--entitlements` file output mode (only embedding)
- No seal-of-seal support
- No designated requirement auto-generation with team ID
- No `--deep` recursive signing
- No `--timestamp` support
- No `--allocation` support for large files
- No universal binary partial signing (signs all architectures together)
- No `--timestamp` option for notarization workflow
- No `--generate-required` or complex requirement specification

### 2.2 devicectl

**Source:** `src/xcode/devicectl/` (2 files: devicectl.c, devicectl.h)

**Capabilities:**
- Device listing and pairing
- USB multiplexing
- Device info retrieval
- Basic device management operations

**What's missing compared to Apple's devicectl:**
- No remote device management
- No app installation/removal via devicectl
- No screen recording/streaming
- No `device.process` subsystem (process management)
- No `device.pairing` full lifecycle
- No iOS/tvOS simulation control
- No file push/pull operations
- No log streaming
- No crash log retrieval

### 2.3 notarytool

**Source:** `src/xcode/notarytool/` (6 files: notarytool.c, api.c, api.h, json.c, json.h, jwt.c, jwt.h, keychain.c, keychain.h)

**Capabilities:**
- Notarization submission via Apple API
- JWT-based authentication (App Store Connect API keys)
- JSON response parsing
- Keychain integration for credential lookup
- Progress tracking

**What's missing compared to Apple's notarytool:**
- No `notarytool history` command (history of past submissions)
- No `notarytool log` command (detailed submission logs)
- No `notarytool cancel` (cancel in-progress submissions)
- No `--key-path` / `--key-id` / `--issuer` shortcut options (only via keychain)
- No Stapler integration (automatic stapling after notarization)
- No `--no-progress` or `--progress` formatting options
- No submission ID tracking or metadata
- No batch submission support

### 2.4 pkgbuild

**Source:** `src/xcode/pkgbuild/` (6 files: pkgbuild.c, analyze.c, analyze.h, payload.c, payload.h, bom.c, bom.h, xar.c, xar.h)

**Capabilities:**
- `.pkg` installer creation from file payloads
- xar archive generation (custom implementation, not using `/usr/bin/xar`)
- BOM (Bill of Materials) generation
- cpio-formatted payload archives
- PackageInfo XML generation
- `--analyze` mode (prints PackageInfo)
- `--inspect` mode (lists archive contents)
- `--sign` support (codesign integration)
- `--scripts` for pre/post-install scripts
- Component-based package building
- File permission preservation
- `--root` mode for directory-based payloads

**What's missing compared to Apple's pkgbuild:**
- No `--root` with payload path mapping (complex path remapping)
- No `--component` with full `.app` bundle analysis
- No `--scripts` validation (pre/post install script syntax checking)
- No `--relocatable` package support
- No `--filter` for file filtering
- No `--version` with full semver support
- No `--owner`, `--group`, `--mode` for individual files
- No `--compression-level` control
- No `--legacy` flag for flat packages
- No `--timestamp` for reproducible builds
- No `--install-location` for relocatable installers
- No `--preserve-plists` or complex plist options

### 2.5 productbuild

**Source:** `src/xcode/productbuild/` (7 files: productbuild.c, dist.c, dist.h, analyze.c, analyze.h, payload.c, payload.h, bom.c, bom.h, xar.c, xar.h)

**Capabilities:**
- Product archive (`.pkg`) creation from distribution XML
- Distribution XML parsing
- Product build from multiple components
- xar archive generation (shared with pkgbuild)
- BOM and payload generation
- `--analyze` mode
- `--sign` support
- `--package` for component-based builds
- `--product` for product definition
- Volume selection for installation

**What's missing compared to Apple's productbuild:**
- No `--compile` to create distribution from Xcode project
- No `--export` for App Store exports
- No `--sign` with timestamp/notharmless
- No `--distribution` with full XML schema support
- No `--pkg` for combining multiple packages
- No `--installer-version` for compatibility
- No `--min-product-version` check
- No `--sandbox` for building in sandbox
- No localization support for distribution
- No `--scripts` for distribution-level scripts

### 2.6 simctl

**Source:** `src/xcode/simctl/` (5 files: simctl.c, simctl.h, sim_list.c, sim_list_dispatch.c, sim_list_dispatch.c, sim_ops.c)

**Capabilities:**
- Simulator listing (available devices)
- Simulator creation and deletion
- Simulator boot/shutdown
- Simulator spawn operations
- Device type enumeration
- Runtime management
- Device state queries
- JSON output formatting

**What's missing compared to Apple's simctl:**
- No `simctl io` (screen recording, media capture)
- No `simctl push`/`pop` (push notifications)
- No `simctl location` (simulating location)
- No `simctl spawn` with full process control
- No `simctl file` operations (push/pull files)
- No `simctl openurl` for URL scheme testing
- No `simctl env` for environment variable injection
- No `simctl status_bar` (status bar customization)
- No `simctl diagnose` (diagnostic collection)
- No `simctl monitor` (event streaming)
- No `simctl shutdown all` with force
- No UI automation support (`simctl ui`)
- No `simctl boot` with device type specification
- No `simctl create` with device type specification
- No integration with Xcode Server

### 2.7 xcode-select

**Source:** `src/xcode/xcode-select/` (1 file: xcode-select.c)

**Capabilities:**
- `--switch <path>` to select developer directory
- `--print-path` to show current developer directory
- `--version` to display version
- `--help` for usage
- `DEVELOPER_DIR` environment variable override
- Persistent storage in `~/.xcdev.dat`

**What's missing compared to Apple's xcode-select:**
- No `--install` for Xcode command-line tools installation
- No `--reset` to reset to system default
- No `--sdk` to list available SDKs
- No `--toolchain` to list/select toolchains
- No path validation (doesn't verify the path is valid)
- No `--list` option for all installed developer directories
- No `XDToolchain` selection support
- No integration with system preferences

### 2.8 xcodebuild

**Source:** `src/xcode/xcodebuild/` (6 files: xcodebuild.c, project.c, project.h, plist.c, plist.h, settings.c, settings.h, ini.c, ini.h)

**Capabilities:**
- Developer directory resolution (via xcode-select, DEVELOPER_DIR)
- SDK and toolchain metadata parsing (info.ini format)
- Xcode project (`.pbxproj`) parsing (minimal NextSTEP plist parser)
- `.xcconfig` file parsing (with `#include` / `#include?` support)
- Variable expansion (`$(VAR)` / `${VAR}`)
- Build settings table resolution
- `-showBuildSettings` (text and JSON output)
- `-list` (targets, configurations, schemes)
- `-showsdks` (list available SDKs and toolchains)
- Build actions (`build`, `test`, `analyze`, `archive`, `install`, `installsrc`, `clean`, `run`, `bench`, `test-without-building`)
- `-dry-run` to print delegation commands without executing
- `-exportArchive` with exportOptions.plist validation
- Toolchain delegation via `xcrun`

**What's missing compared to Apple's xcodebuild:**
- No actual file compilation (delegates to toolchain's build driver)
- No Swift compilation support
- No C++ compilation support
- No per-file compilation flags
- No `-alltargets` / `-allconfigs` support
- No `-target <name>` for specific target selection
- No `-configuration <name>` selection
- No `-sdk <name>` specification
- No `-toolchain <name>` selection
- No `-derivedDataPath` for custom DerivedData location
- No `-archivePath` for archive output
- No `-scheme` for scheme-based building
- No `-destination` / `-destination-for-build`
- No `-xcconfig` full support
- No `-x <language>` to override file type
- No `-dry-run` with actual dependency graph resolution
- No `-quiet` or `-silent` modes
- No progress reporting
- No parallel build support (`-j`)
- No build log output formatting
- No `-resultBundlePath` for test results
- No integration with XCTest
- No `--quiet`/`--verbose` logging levels

### 2.9 xcrun

**Source:** `src/xcode/xcrun/` (3 files: xcrun.c, ini.c, ini.h)

**Capabilities:**
- Tool location within Developer folder
- SDK-specific tool resolution
- Toolchain-specific tool resolution
- `--find <tool>` to print tool path
- `--run <tool>` to find and execute
- `--sdk <sdk>` for SDK-specific tools
- `--toolchain <name>` for toolchain-specific tools
- `--show-sdk-path`, `--show-sdk-version`, `--show-sdk-target-triple`
- `--show-sdk-toolchain-path`, `--show-sdk-toolchain-version`
- `-v` / `--verbose` mode
- `-l` / `--log` mode (display commands)
- Environment variable setup (SDKROOT, PATH, TARGET_TRIPLE, deployment targets)
- Configuration via `/etc/xcrun.ini`
- Multicall behavior (symlinks to xcrun for tool wrapping)

**What's missing compared to Apple's xcrun:**
- No `--run-script` mode
- No `--toolchain-path` display
- No `--sdk` with platform specification
- No `--log` with detailed debug output
- No `--find` with multiple toolchains
- No `--launch-browser` or URL handling
- No integration with Xcode Server
- No `--no-cache` to bypass cached tool paths
- No `--resolve` for resolving ambiguous tool names
- No support for `xcrun` as a library (no libxcrun)
- No `--package` mode for package tools
- No `--internal-` prefixed internal commands
- Limited `DEVELOPER_DIR` resolution (no `/usr/lib` fallback)

### 2.10 xctrace

**Source:** `src/xcode/xctrace/` (4 files: xctrace.c, xctrace.h, xc_record.c, xc_export.c)

**Capabilities:**
- `.trace` file recording initiation
- Trace file export to supported formats
- Export configuration parsing
- Record session management

**What's missing compared to Apple's xctrace:**
- No `xctrace record` with actual tracing (DTTrace integration)
- No `xctrace list` (list available trace templates)
- No `--template` for template-based recording
- No `--device` specification for recording target
- No `--time-limit` for recording duration
- No `--output` for specifying output file
- No `--attach` for attaching to running process
- No `--launch` for launching app for tracing
- No `--target` for specific process targeting
- No `xctrace export` with full format support (only basic export)
- No `xctrace dump-ts` for timestamp analysis
- No `xctrace import` for importing trace files
- No `--summarize` or `--timescope` options
- No `--attach-to` or `--pid` support
- No `xctrace print` for console output of trace contents
- No `xctrace record --template` for custom templates

## 3. Tools in Xcode Developer Directory That We Don't Have

### 3.1 Build & Compilation Tools (99 in usr/bin, 119 in toolchain)

| Tool | Apple Location | Our Source | Status |
|------|---------------|------------|--------|
| clang | Toolchain usr/bin | `src/llvm-project/clang/` | ✅ Source available (submodule) |
| swiftc | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-frontend | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-driver | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-build | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-package | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-run | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-test | Toolchain usr/bin | `src/swift/` | ✅ Source available (submodule) |
| swift-demangle | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| swift-api-digester | Toolchain usr/bin | `src/swift/` | ✅ Source available |
| swift-format | Toolchain usr/bin | `src/swift/` | ✅ Source available |
| clang-format | Toolchain usr/bin | `src/llvm-project/clang-tools-extra/` | ✅ Source available |
| clangd | Toolchain usr/bin | `src/llvm-project/clang-tools-extra/` | ✅ Source available |
| ld | Toolchain usr/bin | `src/dist-dev-tools/ld64/` | ✅ Source available (submodule) |
| ld-classic | Toolchain usr/bin | `src/dist-dev-tools/ld64/` | ✅ Source available |
| ar | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| libtool | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| lipo | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| otool | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| nm | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| ranlib | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| strip | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| dsymutil | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| dwarfdump | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| bitcode_strip | Toolchain usr/bin | `src/dist-dev-tools/cctools/` | ✅ Source available |
| llvm-cov | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-profdata | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-nm | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-objdump | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-otool | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-size | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-cxxfilt | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-readtapi | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| llvm-stat-cache | Toolchain usr/bin | `src/llvm-project/llvm/` | ✅ Source available |
| tapi | Toolchain usr/lib | `src/dist-dev-tools/tapi/` | ✅ Source available |
| make | usr/bin | `src/dist-dev-tools/gnumake/` | ✅ Source available |
| gnumake | usr/bin | `src/dist-dev-tools/gnumake/` | ✅ Source available |
| g++ | usr/bin | `src/llvm-project/` (via clang) | ✅ Source available |
| gcc | usr/bin | `src/llvm-project/` (via clang) | ✅ Source available |
| bison | Toolchain usr/bin | `src/dist-dev-tools/bison/` | ✅ Source available |
| flex | Toolchain usr/bin | `src/dist-dev-tools/flex/` | ✅ Source available |
| gm4 | Toolchain usr/bin | `src/dist-dev-tools/gm4/` | ✅ Source available |
| gperf | Toolchain usr/bin | `src/dist-dev-tools/gperf/` | ✅ Source available |

### 3.2 Apple-Specific Tools (No Open Source Equivalents)

| Tool | Category | Description |
|------|----------|-------------|
| actool | Asset Catalog | Compiles `.xcassets` into compiled asset catalogs |
| ibtool | Interface Builder | Compiles `.xib`/`.xib` files into `.nib` |
| ibtoold | Interface Builder | Daemon version of ibtool |
| coremlc | Core ML | Compiles Core ML models |
| momc | Core Data | Compiles `.xcdatamodel` files |
| mapc | Maps | Map template compiler |
| copypng | Asset | PNG optimization/copy for iOS resources |
| pngcrush | Asset | PNG optimization | ✅ Source available (`src/pngcrush/`) |
| TextureAtlas | Asset | Texture atlas compiler |
| TextureConverter | Asset | Texture format conversion |
| altool | App Store | App Store Transport / upload validation |
| iTMSTransporter | App Store | App Store delivery tool |
| bitcode-build-tool | Build | Bitcode linking tool |
| atos | Debug | Address-to-symbolication |
| vmmap | Debug | Virtual memory map analysis |
| symbols | Debug | Symbol table extraction |
| leaks | Debug | Memory leak detection |
| malloc_history | Debug | Malloc allocation history |
| heap | Debug | Heap analysis |
| lldb | Debug | Debugger (we have source via llvm-project) |
| lldb-dap | Debug | Debug Adapter Protocol server |
| malloc_history | Debug | Memory history tracking |
| xcdebug | Debug | Debug log analysis |
| xcindex-test | Debug | Index testing |
| gcov | Coverage | Coverage data processing |
| headerdoc2html | Docs | Header documentation generator |
| hdxml2manxml | Docs | Header docs to man page XML |
| sdef | Scripting | Scripting definition file generator |
| sdp | Scripting | Scripting definition compiler |
| scntool | SceneKit | SceneKit asset tool |
| compileSceneKitShaders | SceneKit | SceneKit shader compiler |
| copySceneKitAssets | SceneKit | SceneKit asset copier |
| realitytool | AR | RealityKit asset tool |
| referenceobjectc | AR | AR reference object compiler |
| ictool | Asset | Asset catalog inspection |
| instrumentbuilder | Instruments | Trace template builder |
| agvtool | Version | Version string management |
| cktool | Code Signing | Key management tool |
| ipatool | App Store | IPA packaging tool |
| ipatool2 | App Store | IPA packaging (v2) |
| iphoneos-optimize | Asset | iOS optimization tool |
| placeholderutil | App Store | Placeholder image utility |
| xarsigner | Archive | xar signing tool |
| xccov | Testing | Coverage report generator |
| xcresulttool | Testing | Test result processing |
| xcsigningtool | Code Signing | Signing identity tool |
| xcstringstool | Localization | String manipulation tool |
| xctest | Testing | Unit test runner |
| xed | IDE | Source editor (GUI) |
| xml2man | Docs | Man page generator |
| agent | Agent | Background service management |
| ba-package | Build | Build assistant packaging |
| ba-serve | Build | Build assistant server |
| backgroundassets-debug | Debug | Background assets debugging |
| compositeMD5 | Archive | MD5 computation |
| convertRichTextToAscii | Conversion | Rich text to ASCII |
| DeRez | Resource | Resource fork de-compiler |
| Rez | Resource | Resource compiler |
| ResMerger | Resource | Resource fork merger |
| GetFileInfo | File | File metadata query |
| SetFile | File | File metadata setter |
| SplitForks | File | Fork splitting tool |
| resolveLinks | File | Symbolication link resolver |
| swinfo | File | Swift info query |
| stringdups | File | String deduplication |
| filtercalltree | Debug | Call tree filtering |
| extractLocStrings | Localization | Localization string extraction |
| gatherheaderdoc | Docs | Header doc gathering |
| desdp | Debug | Dispatch profiling tool |
| desdp | Debug | Dispatch profiling tool |
| stapler | Code Signing | Signature stapling |
| embeddedBinaryValidationUtility | Code Signing | Embedded binary validation |
| crashlog | Debug | Crash log management |
| CreateIPA | App Store | IPA creation |
| momc | Core Data | Managed object model compiler |
| 2to3 | Python | Python 2 to 3 converter | ✅ Source available |
| pip3 | Python | Python package installer | ✅ Part of CPython source |
| pydoc3 | Python | Python documentation | ✅ Part of CPython source |
| python3 | Python | Python 3 interpreter | ✅ Source available (`src/cpython/` + `src/python-apple-support/`) |
| Make | Python | Make helper | ✅ Part of CPython source |
| Git | VCS | Git operations | ✅ Source available (`src/git/`) |
| Git-receive-pack | VCS | Git receive pack | ✅ Source available (`src/git/`) |
| Git-shell | VCS | Git shell | ✅ Source available (`src/git/`) |
| Git-upload-archive | VCS | Git upload archive | ✅ Source available (`src/git/`) |
| Git-upload-pack | VCS | Git upload pack | ✅ Source available (`src/git/`) |

### 3.3 Tools in /Applications/Xcode.app/Contents/Developer/Tools/

| Tool | Description |
|------|-------------|
| DeRez | Resource de-compiler |
| GetFileInfo | File info query |
| ResMerger | Resource merger |
| Rez | Resource compiler |
| SetFile | File attribute setter |
| SplitForks | Fork splitter |

### 3.4 Toolchain Tools (119 in XcodeDefault.xctoolchain/usr/bin/)

**Compiler & Language Tools (we have via llvm-project + swift submodules):**
- clang, clang++, cc, c++ (LLVM/Clang)
- swift, swiftc, swift-frontend, swift-driver (Swift)
- swift-build, swift-package, swift-run (Swift Package Manager)
- swift-format, swift-api-digester (Swift tooling)
- clang-format, clangd (Clang tooling)
- llvm-* family (LLVM utilities)

**Language Support Tools (we have via submodules):**
- sourcekit-lsp (Swift LSP, via llvm-project)
- swift-demangle (via llvm-project)
- libSwiftSourceKit (via swift submodule)

**Build & Assembly Tools (we have via dist-dev-tools):**
- ar, ranlib, libtool, ld (via ld64/cctools)
- lipo, otool, nm (via cctools)
- dsymutil (via llvm-project)
- bitcode_strip (via cctools)

**Tools we DON'T have (no source, Apple-internal):**
- appintentsmetadataprocessor
- appintentsnltrainingprocessor
- appshortcutstringsprocessor
- cache-build-session
- clang-cache
- clang-cas-test
- createml
- exutil
- iig
- llvm-cas
- metal
- metal-package-builder
- mig
- modules-verifier
- snippet-extract
- swift-experimental-sdk
- swift-package-collection
- swift-package-registry
- swift-plugin-server
- swift-stdlib-tool
- swift-static
- swift-symbolgraph-extract
- swift-synthesize-interface
- unifdef, unifdefall
- vtool
- unwinddump
- c89, c99

### 3.5 SDK & Platform Infrastructure

**SDKs we don't have yet:**
- AppleTVOS.sdk
- AppleTVSimulator.sdk
- DriverKit.sdk
- iPhoneOS.sdk
- iPhoneSimulator.sdk
- MacOSX.sdk (multiple versions: MacOSX.sdk, MacOSX26.5.sdk, MacOSX26.sdk)
- WatchOS.sdk
- WatchSimulator.sdk
- XROS.sdk
- XRSimulator.sdk

**Platform directories we don't have:**
- All 10 platform directories under `Platforms/`

## 4. Toolchain Component Coverage

### llvm-project Submodule

**Contents:** The upstream LLVM/Clang project, including:
- LLVM core (optimizer, code generator, IR)
- Clang (C/C++/ObjC compiler frontend)
- Clang tools (clang-format, clangd, clang-tidy)
- compiler-rt (runtime compiler libraries)
- libcxx, libcxxabi (C++ standard library)
- libunwind
- LLD (linker)
- LLDB (debugger)
- MLIR
- Flang (Fortran compiler)
- Polly (optimizer)
- OpenMP runtime

**What it covers from the Xcode toolchain:**
- All clang/clang++ functionality
- LLVM-based tools (llvm-nm, llvm-objdump, etc.)
- DWARF debugging support (llvm-dwarfdump, dsymutil)
- LTO support (libLTO)
- Sanitizer runtimes (via compiler-rt)

**Gaps:**
- Swift compiler (separate submodule)
- Apple-specific Clang extensions (CAS, caching)
- `clang-cache` and `clang-cas-test` (Apple's Caching Clang)
- Metal compiler
- MIG (requires dist-dev-tools)

### swift Submodule

**What it covers:**
- Swift compiler (swiftc)
- Swift frontend (swift-frontend)
- Swift driver (swift-driver)
- Swift Package Manager (swift-package, swift-build, swift-run)
- Swift REPL
- Swift demangler (swift-demangle)
- SourceKit (sourcekitd, sourcekit-lsp)

**Gaps:**
- Swift experimental SDK support
- Swift package registry client
- Swift plugin server
- Swift symbol graph extraction (partially covered)

### dist-dev-tools Submodule

**What it covers:**
- cctools (ar, ranlib, strip, lipo, otool, nm, etc.)
- ld64 (the Apple linker, ld, ld-classic)
- GNU tools (bison, flex, gperf, gnumake, gm4)
- headerdoc (documentation generator)
- tapi (text-based API tools)
- developer_cmds (various)
- bootstrap_cmds
- libgit2

**Gaps:**
- Missing `vtool`, `unifdef`, `mig`, `segedit`
- Missing `dsymutil` (use LLVM's instead)

### objc4 Submodule

**What it covers:**
- Objective-C runtime library
- ARC support
- Blocks runtime

**Gaps:**
- No Objective-C compiler (covered by clang in llvm-project)
- No runtime headers for all platforms

### pngcrush Submodule

**Source:** `src/pngcrush/` (pngcrush v1.8.1, xnuports fork)

**What it covers:**
- PNG (Portable Network Graphics) optimization
- PNG IDAT datastream compression optimization
- Removal of unwanted ancillary chunks
- Adding gAMA, tRNS, iCCP, and textual chunks
- Bundled with libpng and zlib source (modified fork)
- Pre-compiled binary available in submodule
- Batch processing scripts for workspace-based workflows

**Gaps:**
- No Makefile checked out in current commit (build files exist in git history)
- Pre-compiled `.o` files and binary in tree (should be cleaned)
- Uses bundled (modified) libpng/zlib rather than system libraries
- No integration into our bmake build system yet

### git Submodule

**Source:** `src/git/` (Git v2.55.0, xnuports fork)

**What it covers:**
- Full Git source code (git, git-receive-pack, git-shell, git-upload-archive, git-upload-pack)
- Apple's Xcode ships Git in its Developer/usr/bin — our source matches the same Git version
- Includes `libgit2` is already available in dist-dev-tools (as a library)
- Note: Apple's git includes Apple-specific patches (credential helpers, keychain integration)

**Gaps:**
- May lack Apple-specific patches present in Xcode's build
- No Swift-specific Git integration (e.g., `swift package`-style Git helpers)

### cpython Submodule

**Source:** `src/cpython/` (CPython 3.14.7, xnuports fork on `3.14` branch)

**What it covers:**
- Full CPython 3.14.7 source code
- Python 3 interpreter (`python3`)
- pip, pydoc, 2to3, and other Python tools
- Apple-specific directory structure (`src/cpython/Apple/`, `src/cpython/iOS/`)
- Cross-compilation toolchain resources for iOS (`src/cpython/iOS/Resources/bin/`)

**What it doesn't cover:**
- Building fat binaries for multiple Apple platforms (handled by python-apple-support)
- App Store compliance patches (handled by python-apple-support)
- SDK-specific configurations (handled by python-apple-support)

### python-apple-support Submodule

**Source:** `src/python-apple-support/` (Beeware Python-Apple-support, `heads/main`)

**What it covers:**
- Meta-build system for packaging Python as XCFrameworks for Apple platforms
- Downloads, patches, and builds CPython 3.14.6 with dependencies:
  - bzip2 1.0.8, libffi 3.4.7, mpdecimal 4.0.0, OpenSSL 3.5.7, xz 5.6.4, zstd 1.5.7
- Builds fat binaries supporting macOS (x86_64, arm64), iOS, tvOS, watchOS, visionOS
- Creates relocatable frameworks for embedding in Xcode projects
- App Store compliance patches applied to macOS build
- Cross-platform patches for iOS/tvOS/watchOS/visionOS (backported from PEP 730)
- Reuses official macOS Python binary packages (re-packaged as XCFramework)

**Relationship to cpython submodule:**
- `src/cpython/` contains the actual CPython source tree
- `src/python-apple-support/` is the build system that compiles and packages it
- They are complementary: cpython provides source, python-apple-support provides the Apple-platform build pipeline

**Gaps:**
- Not yet integrated into our bmake build system
- No Makefile for building (uses its own Makefile which calls `xcodebuild`/`cmake`)

### include/ld-internals

**Source:** `include/ld-internals` (git submodule, `heads/main`)

- Apple internals header files for `ld64`
- Private APIs and data structures used by the Apple linker

## 5. Configuration Infrastructure

### Our configs/ directory
- `DarwinARMSDKSettings.info.ini` — SDK configuration (name, version, arch, toolchain, deploy target)
- `DarwinARMToolchainSettings.info.ini` — Toolchain configuration
- `xcrun.ini` — Default SDK/toolchain selection
- `Makefile` — Install target for configs
- Uses INI format (simpler than Apple's plist-based configs)

### Apple's configuration infrastructure
- `SDKSettings.plist` — Per-SDK settings (property list format, far more detailed)
- `ToolchainInfo.plist` / `.xcconfig` — Toolchain settings
- Platform `.platform` bundles with `Info.plist`, `SDKs/`, `Toolchains/`
- `xcrun.ini` at `/etc/xcrun.ini` (system-wide)
- `xcodebuild` configuration files (`xcodebuild -configuration` files)
- Build system settings in `Makefiles/` directory

## 6. Build System

### Our Build System
- **bmake** (BSD make) as the build system
- Hierarchical Makefile structure:
  - `Makefile` → `src/Makefile` → `src/xcode/Makefile` → `src/xcode/<tool>/Makefile`
- All build artifacts go to `build/` directory:
  - `build/obj/<tool>/` — object files
  - `build/usr/bin/` — final binaries
- Tool Makefiles use `.for` loops for explicit compile rules
- Uses `!=` operator instead of `$(shell ...)` for command substitution
- Uses `${.CURDIR}` instead of `$(CURDIR)` for directory detection

### Apple's Build System
- Custom Xcode build system (`xcodebuild`)
- Xcode project files (`.xcodeproj`)
- `.xcconfig` configuration files
- Xcode build settings (extensive, with hundreds of variables)
- Build phases (compile, link, copy, run script, etc.)
- DerivedData management
- Build server integration

## 7. Comparison Matrix

| Capability | Our Status | Apple | Notes |
|-----------|------------|-------|-------|
| Code signing (ad-hoc) | ✅ Working | ✅ Full | Ad-hoc only, no cert-based signing |
| Code signing (cert-based) | ❌ | ✅ | No CMS signature support |
| Build orchestration | ✅ Partial | ✅ Full | Delegates to toolchain for actual compilation |
| Package building | ✅ Good | ✅ Full | Missing some edge cases |
| Product building | ✅ Good | ✅ Full | Missing some distribution options |
| Simulator control | ✅ Partial | ✅ Full | Missing I/O, push, location features |
| Developer dir selection | ✅ Good | ✅ Full | Missing `--install` for CLT |
| Tool location | ✅ Good | ✅ Full | Missing some SDK resolution features |
| Notarization | ✅ Partial | ✅ Full | Missing history/log/cancel |
| Trace recording | ✅ Stub | ✅ Full | No actual tracing backend |
| Compiler (C/C++/ObjC) | ✅ Via submodule | ✅ Full | LLVM/Clang open source |
| Swift compiler | ✅ Via submodule | ✅ Full | Open source Swift |
| Asset compilation | ❌ | ✅ Full | actool, TextureAtlas, etc. |
| IB compilation | ❌ | ✅ Full | ibtool, ibtoold |
| Resource fork tools | ❌ | ✅ Full | Rez, DeRez, SetFile, etc. |
| Debugging tools | ❌ | ✅ Full | lldb, leaks, vmmap, etc. |
| App Store delivery | ❌ | ✅ Full | altool, iTMSTransporter, ipatool |
| Coverage tools | ❌ | ✅ Full | xccov, llvm-cov |
| Localization | ❌ | ✅ Full | genstrings, actool, etc. |
| Python tools | ✅ Source available | ✅ Full | CPython 3.14.7 + build system |
| Git | ✅ Source available | ✅ Full | Git v2.55.0 |

## 8. Roadmap Recommendations

### Phase 1: Toolchain & Infrastructure
1. Build and integrate the LLVM toolchain (clang, swiftc, ld, ar, etc.) from submodules
2. Integrate dist-dev-tools (bison, flex, gnumake, gperf, etc.)
3. Build and install CPython 3.14.7 (`src/cpython/`) using `python-apple-support` build system
4. Build and install Git v2.55.0 (`src/git/`)
5. Set up SDK directories with info.ini files
6. Implement the toolchain shim scripts in `scripts/`
7. Install configs to system paths

### Phase 2: Missing Xcode Tools
1. **actool** — Asset catalog compiler (high priority for iOS/macOS apps)
2. **ibtool** — Interface Builder compilation
3. **coremlc** — Core ML model compilation
4. **momc** — Core Data model compiler
5. **altool/iTMSTransporter** — App Store delivery
6. **lldb** — Debugger (integrate from llvm-project)
7. **stapler** — Signature stapling (for notarization workflow)
8. **agvtool** — Version string management

### Phase 3: Developer Tools
1. **atos, vmmap, symbols, leaks** — Debugging and profiling
2. **ibtoold** — Interface Builder daemon
3. **xcsigningtool** — Signing identity management
4. **xccov** — Coverage reporting
5. **xcresulttool** — Test result processing
6. **ipatool** — IPA packaging tool

### Phase 4: File & Resource Tools
1. **Rez/DeRez/SetFile/GetFileInfo** — Resource fork tools (available in `Tools/`)
2. **copypng** — PNG optimization for iOS resources
3. **sdef/sdp** — Scripting definition tools
4. **TextureAtlas/TextureConverter** — Texture processing

## 9. License Compliance

| Component | License | Source |
|-----------|---------|--------|
| Our xcode/ tools (codesign, devicectl, etc.) | BSD-3-Clause | `src/xcode/` |
| LLVM/Clang | Apache-2.0 + LLVM Exception | `src/llvm-project/` |
| Swift | Apache-2.0 + BSD Runtime | `src/swift/` |
| Objective-C runtime | Apple Public Source License | `src/objc4/` |
| PlistBuddy | Apple Public Source License | `src/PlistBuddy/` |
| dist-dev-tools | Mixed (Apple APL, GPL, BSD) | `src/dist-dev-tools/` |
| Git | GPL-2.0 | `src/git/` |
| CPython | PSF-2.0 | `src/cpython/` |
| Python-Apple-Support | BSD-3-Clause | `src/python-apple-support/` |
| ld-internals | Apple Public Source License | `include/ld-internals/` |
| pngcrush | PNG License | `src/pngcrush/` |
| Our top-level code | BSD-3-Clause | `LICENSE.BSD-3` |
