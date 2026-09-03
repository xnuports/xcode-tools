# xpc

The public `xpc/*.h` headers, written from the documented API.

XPC is macOS's interprocess communication library. The implementation is
in the system's `libSystem`, which exports 547 `_xpc_*` symbols, so
nothing here is compiled and nothing needs to be: a program built
against these headers links against Apple's real libxpc exactly as it
would against Apple's own headers. Apple publishes no source for libxpc
and ships no open-source release of the headers either, so they are
written here.

## Why not take them from an existing reimplementation

There are several — OpenXPC (Airyx/ravynOS), the NextBSD lineage that
`darlinghq/darling-libxpc` and `barracuda156/libxpc` both descend from,
and PureDarwin's. All of them target hosts that have no XPC at all and
replace the transport, usually with D-Bus, because those hosts have
neither Mach nor launchd. That is the right decision there and it makes
their internals the wrong shape for this: the part they rewrote is the
part a Darwin drop-in has to keep.

More to the point, these declare an interface that then binds to
*Apple's* implementation. A prototype that differs from Apple's compiles
cleanly, links against the real libxpc, and goes wrong at runtime — a
silent ABI mismatch, which is worse than having no header. So the
declarations here follow the documented public API, and the
reimplementations above are useful for noticing an omission rather than
for copying.

## What is here

The object model, the container and primitive types, connections and
endpoints -- which is what code using XPC actually calls.

`xpc_object_t` is declared with `OS_OBJECT_DECL`, so XPC objects are
`os_object`s like `dispatch_object_t` and `os_log_t`: under
`OS_OBJECT_USE_OBJC` they are Objective-C objects, and that is a fact
about the ABI rather than a choice made here.

The newer interfaces -- sessions, listeners, activities, rich errors,
peer requirements -- are declared far enough for `<xpc/xpc.h>` to
include them and for the types to exist. They are not complete, and the
header says so where it stops rather than implying otherwise.
