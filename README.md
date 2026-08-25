xcode-tools
===========
An open source reimplementation of Apple's Xcode command-line developer tools.

Our goal is to produce a fully open-source SDK bundle that is function-to-function
identical to Apple's proprietary Xcode Developer tools. All code follows Apple's
open source releases (APSL/GPL/BSD/Apache where applicable) and our own BSD-licensed
reimplementations where Apple has not released source.

## Currently Built (25 programs)

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

Build system: **bmake** (BSD make), with one generic engine driven by an
inventory — the same architecture as the sibling
[apple-core](https://github.com/xnuports/apple-core) project:

```
Makefile → src/Makefile → mk/tool.mk   (once per entry in mk/progs.mk)
```

`mk/progs.mk` lists every program as `<src-dir> <program> <install-suffix>`;
`mk/tool.mk` builds it; `mk/tool.d/<program>.mk` carries any per-tool flags.
Sources belonging to submodules are compiled in place, read-only — nothing is
ever written inside a submodule.

Build artifacts go to `build/`, and `build/release/` is a drop-in replacement for
Xcode's `Developer/` directory.

### Binary Tools (from `src/dist-dev-tools/cctools`)

Built and installed to `Toolchains/XcodeDefault.xctoolchain/usr/bin`, matching
where Xcode ships them:

`ar`, `bitcode_strip`, `codesign_allocate`, `ctf_insert`, `install_name_tool`,
`libtool` (plus `ranlib`), `lipo`, `nm-classic`, `nmedit`, `otool-classic`,
`segedit`, `size-classic` (plus `size`), `strings`, `strip`, `vtool`

Output is verified against Apple's counterparts. The `-classic` names are
Apple's own: in a stock toolchain `nm` and `otool` are symlinks to `llvm-nm` /
`llvm-otool`, with the cctools builds shipped alongside as `nm-classic` and
`otool-classic`.

`libtool` needs `make_obj_file_with_linker_options()` from Apple's
`libcctoolshelper`, which ships in neither the open-source release nor Xcode
itself. `src/cctools-helpers/` is our BSD-licensed reimplementation of it, so
`libtool -ref-l` / `-ref-framework` work; the cctools submodule is untouched.

### Submodule Components

| Component | Source Path | Description | Coverage |
|-----------|-------------|-------------|----------|
| **llvm-project** | `src/llvm-project/` | LLVM/Clang/LLDB/MLIR | C/C++/ObjC compiler, LLVM tools |
| **swift** | `src/swift/` | Swift compiler | swiftc, swift-frontend, SPM, SourceKit |
| **objc4** | `src/objc4/` | Objective-C runtime | Runtime library |
| **pngcrush** | `src/pngcrush/` | PNG optimization | pngcrush v1.8.1 |
| **dist-dev-tools** | `src/dist-dev-tools/` | Apple open-source dev tools (nested submodules) | cctools, ld64, bison, flex, gnumake, gperf, developer_cmds, headerdoc, tapi, pb_makefiles |
| **git** | `src/git/` | Git v2.55.0 | git, git-receive-pack, git-shell, etc. |
| **cpython** | `src/cpython/` | CPython 3.14.7 | python3, pip3, pydoc3, 2to3 |
| **python-apple-support** | `src/python-apple-support/` | Python build system for Apple platforms | XCFramework packaging, cross-platform builds |
| **PlistBuddy** | `src/PlistBuddy/` | Plist manipulation tool | Property list editing |
| **xctoolchain** | `xctoolchain/` | Xcode toolchain configs | Build configurations |
| **ld-internals** | `include/ld-internals/` | Apple linker internals | ld64 private APIs |

## Missing Tools vs. Full Xcode

See `docs/DOCUMENTATION.md` for a comprehensive audit. Key gaps:

- **Asset compilation:** No `actool` (asset catalogs), `TextureAtlas`, `copypng`
- **Interface Builder:** No `ibtool`/`ibtoold` (nib/xib compilation)
- **App Store delivery:** No `altool`, `iTMSTransporter`, `ipatool`
- **Core ML:** No `coremlc` (model compilation)
- **Core Data:** No `momc` (model compilation)
- **File resources:** No `Rez`/`DeRez`/`SetFile`/`GetFileInfo`
- **Debugging:** No `leaks`, `vmmap`, `atos`, `symbols`
- **Code signing:** No certificate-based or CMS signing (ad-hoc only)
- **Swift toolchain:** Swift submodule available but not yet built/integrated
- **SDKs:** No iOS, watchOS, tvOS, visionOS, or macOS SDKs
- **Not yet built:** LLVM toolchain (clang, swiftc), dist-dev-tools (ar, strip, lipo, otool, ld, bison, flex), Python, Git, pngcrush — sources available in submodules but not yet integrated into bmake build

## Quick Start

```sh
# Build everything in the inventory
bmake

# Print the inventory with install locations
bmake list-progs

# Output lands in build/release/, laid out like Xcode's Developer/
ls build/release/usr/bin/

# Clean build artifacts
bmake clean
```

There is no `install` target: `build/release/` is the product.

```
build/release/usr/bin                                      our tools
build/release/Toolchains/XcodeDefault.xctoolchain/usr/bin  cctools, ld64, clang, swiftc
build/release/Platforms/<P>.platform/Developer/SDKs        SDK bundles
```

Optional tiers:

```sh
bmake MK_TOOLCHAIN=no   # skip the binutils tier (cctools, ld64)
```

## License

Primary code: BSD-3-Clause (see `LICENSE.BSD-3`)
Submodules retain their respective licenses (Apache 2.0, APSL, GPL, BSD, MIT, PSF, PNG License, etc.)
