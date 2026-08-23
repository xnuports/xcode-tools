xcode-tools
===========
An open source reimplementation of Apple's Xcode command-line developer tools.

Our goal is to produce a fully open-source SDK bundle that is function-to-function
identical to Apple's proprietary Xcode Developer tools. All code follows Apple's
open source releases (APSL/GPL/BSD/Apache where applicable) and our own BSD-licensed
reimplementations where Apple has not released source.

## Currently Implemented Tools (10)

### Xcode Command-Line Tools (our reimplementations)

| Tool | Lines of Code | Status |
|------|--------------|--------|
| **codesign** | ~2,500 lines (7 files) | Ad-hoc signing & verification complete |
| **devicectl** | ~650 lines (2 files) | Device listing, pairing, basic management |
| **notarytool** | ~1,200 lines (6 files) | App notarization via Apple API |
| **pkgbuild** | ~900 lines (6 files) | Package (.pkg) creation with xar/BOM/payload |
| **productbuild** | ~1,100 lines (7 files) | Product archive creation from distribution XML |
| **simctl** | ~700 lines (4 files) | Simulator listing, creation, boot/shutdown |
| **xcode-select** | ~250 lines (1 file) | Developer directory selection |
| **xcodebuild** | ~1,200 lines (6 files) | Build orchestration, project parsing, SDK resolution |
| **xcrun** | ~400 lines (3 files) | Tool locator and executor with SDK/toolchain support |
| **xctrace** | ~400 lines (4 files) | Trace recording and export (stub) |

Build system: **bmake** (BSD make), hierarchical Makefiles:
```
Makefile → src/Makefile → src/xcode/Makefile → src/xcode/<tool>/Makefile
```
All build artifacts go to `build/` (`build/usr/bin/` for binaries, `build/obj/` for objects).

### Submodule Components

| Component | Source Path | Description | Coverage |
|-----------|-------------|-------------|----------|
| **llvm-project** | `src/llvm-project/` | LLVM/Clang/LLDB/MLIR | C/C++/ObjC compiler, LLVM tools |
| **swift** | `src/swift/` | Swift compiler | swiftc, swift-frontend, SPM, SourceKit |
| **objc4** | `src/objc4/` | Objective-C runtime | Runtime library |
| **dist-dev-tools** | `src/dist-dev-tools/` | Apple open-source dev tools | bison, flex, gnumake, gperf, ld64, cctools |
| **git** | `src/git/` | Git v2.55.0 | git, git-receive-pack, git-shell, etc. |
| **cpython** | `src/cpython/` | CPython 3.14.7 | python3, pip3, pydoc3, 2to3 |
| **python-apple-support** | `src/python-apple-support/` | Python build system for Apple platforms | XCFramework packaging, cross-platform builds |
| **PlistBuddy** | `src/PlistBuddy/` | Plist manipulation tool | Property list editing |
| **xctoolchain** | `xctoolchain/` | Xcode toolchain configs | Build configurations |
| **ld-internals** | `include/ld-internals/` | Apple linker internals | ld64 private APIs |

## Missing Tools vs. Full Xcode

See `docs/DOCUMENTATION.md` for a comprehensive audit. Key gaps:

- **Asset compilation:** No `actool` (asset catalogs), `TextureAtlas`, `copypng`, `pngcrush`
- **Interface Builder:** No `ibtool`/`ibtoold` (nib/xib compilation)
- **App Store delivery:** No `altool`, `iTMSTransporter`, `ipatool`
- **Core ML:** No `coremlc` (model compilation)
- **Core Data:** No `momc` (model compilation)
- **File resources:** No `Rez`/`DeRez`/`SetFile`/`GetFileInfo`
- **Debugging:** No `leaks`, `vmmap`, `atos`, `symbols`
- **Code signing:** No certificate-based or CMS signing (ad-hoc only)
- **Swift toolchain:** Swift submodule available but not yet built/integrated
- **SDKs:** No iOS, watchOS, tvOS, visionOS, or macOS SDKs
- **Not yet built:** LLVM toolchain (clang, etc.), Python, Git — sources available but not yet integrated into build

## Quick Start

```sh
# Build all tools
bmake

# Output binaries in build/usr/bin/
ls build/usr/bin/

# Clean build artifacts
bmake clean

# Install (requires root)
bmake install
```

## License

Primary code: BSD-3-Clause (see `LICENSE.BSD-3`)
Submodules retain their respective licenses (Apache 2.0, APSL, GPL, BSD, MIT, etc.)
