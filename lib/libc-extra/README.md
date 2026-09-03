# libc-extra

Public C library headers that Apple's Libc open-source release does not
ship, written from the documented public API.

`lib/libc` is Apple's Libc, added as a submodule, and the SDK installs
its public headers from there. A few headers that macOS's SDK carries
are missing from the open-source drop -- Apple omits them -- so they
cannot be installed from the submodule, and the submodule must not be
edited to add them.

They live here instead, as our own BSD-licensed reimplementations of the
public interface. Each declares the same ABI macOS exposes -- the enum
values, struct layouts and function prototypes are facts about the
platform, so a program built against these links against the real
implementation in the system's libSystem exactly as it would against
Apple's own header.

* `sysdir.h` -- the system search-path enumeration API
  (`sysdir_start_search_path_enumeration` and its partner). Absent from
  Libc-1752.120.2. Foundation's FileManager search-path lookups need it.

This is a header-only directory. Nothing here is compiled: the symbols
resolve through the `libSystem` stub at link time.
