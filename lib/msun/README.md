# msun

`math.h` and `complex.h`, vendored from FreeBSD's math library.

Apple publishes no Libm. It is absent from the `distribution-macOS`
release manifest at every version, so these headers — unlike every other
header this tree installs into the SDK — cannot be built from Apple's
open-source releases. Without `math.h` the SDK compiles C but not C++:
libc++'s own `math.h` wraps the C library's, and there is nothing to
wrap. Without `complex.h` it does not reliably compile C either, because
clang's own `tgmath.h` includes `<complex.h>` with no guard — so the gap
stopped anything reaching `<tgmath.h>`, not merely code doing complex
arithmetic.

So these headers are vendored rather than built. They are the files
macOS's own libm descends from: Apple's implementation began as this
code, which is why the interface they declare is the one the platform
provides.

* `math.h` — from `lib/msun/src/math.h` in <https://github.com/freebsd/freebsd-src>
  * License: SunPro (permissive, notice-preserving) — see the file's header
  * Vendored: 2026-09, from `main`
* `complex.h` — from `include/complex.h` in the same repository
  (FreeBSD keeps it in `include/`, not under `lib/msun/`)
  * License: BSD-2-Clause
  * Vendored: 2026-09, from `main`

They are headers only. Nothing here is compiled: the implementations come
from the system's libm at link time, through the `libSystem` stub. These
declare the interface so a compiler can see it.

Update them by replacing the files from the same paths upstream. They have
no local modifications, deliberately — a vendored file that has been
edited is one nobody can update.

## The adapters

`math-darwin.h` and `complex-darwin.h` are ours, not FreeBSD's, and are
what actually get installed as the SDK's `math.h` and `complex.h`; the
vendored files go to `msun/` beside them and are included from there.

Both exist for the same reason. FreeBSD's headers gate their contents on
FreeBSD's visibility macros — `__ISO_C_VISIBLE` and friends, set by
FreeBSD's `<sys/cdefs.h>`. Apple's `cdefs.h` answers the same question
with `__DARWIN_C_LEVEL` and defines none of them, so the vendored headers
parse on this system and then declare almost nothing. That is a quiet
failure, not a loud one, which is why it is worth a file each to prevent.
The adapters set those macros to the levels FreeBSD's `cdefs.h` would
select for a default compilation, each guarded so a program that has
chosen its own level keeps it, and then include the vendored header.

`math-darwin.h` carries one thing beyond that: `__float_t` and
`__double_t`, which FreeBSD declares in `<machine/_types.h>` and Apple has
no equivalent for, derived from the compiler's own `__FLT_EVAL_METHOD__`.
