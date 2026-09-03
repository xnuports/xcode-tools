# swift-darwin-overlay

The Swift overlay for the `Darwin` module, restored from the Swift
repository's history.

`import Darwin` in Swift resolves two things stacked together: the Clang
module declared by the SDK's module map, and a Swift overlay on top of it
that adds what C cannot express. The overlay is where `POSIXErrorCode`
lives, where `open`, `openat` and `fcntl` get non-variadic forms Swift can
actually call, and where `S_IFMT` and friends become `Int32` rather than
untyped macros. Without it a Swift program can name the C functions and
use almost none of them.

Apple ships the overlay prebuilt in the macOS SDK, as
`usr/lib/swift/Darwin.swiftmodule`, and stopped building it from source on
2025-05-22 in `15345ef2d51`, "[CMake][Darwin] Remove support for building
the SDK overlays on Apple platforms". The sources were deleted in that
commit and not moved anywhere; they are still in the history.

These four files are that commit's parent, unmodified:

* `Darwin.swift.gyb` — `@_exported import Darwin`, and the `M_PI` family
* `Platform.swift` — the `open`/`openat`/`fcntl` shims, `errno`,
  `DarwinBoolean`, the `S_IF*` constants
* `POSIXError.swift` — `POSIXErrorCode` and its error domain
* `MachError.swift` — the Mach error domain

* Source: `stdlib/public/Platform/` in <https://github.com/swiftlang/swift>
  at `15345ef2d51^`
* License: Apache 2.0 with Runtime Library Exception — the same licence as
  the Swift standard library this SDK already carries
* Restored: 2026-09

`Darwin.swift.gyb` is a template and is run through the `gyb` in the swift
port's own source tree before compiling, exactly as its build did.

Two things follow from taking a snapshot of a deleted file. It is pinned
to that commit and upstream will not update it, so it has to be kept in
step with the standard library by hand if the compiler moves under it.
And `Platform.swift` refers to `OSStatus`, which lives in `MacTypes.h` —
a CarbonHeaders header Apple does not publish — so `lib/libc-extra`
carries the base type layer of that header for it.
