# msun

`math.h`, vendored from FreeBSD's math library.

Apple publishes no Libm. It is absent from the `distribution-macOS`
release manifest at every version, so `math.h` — unlike every other
header this tree installs into the SDK — cannot be built from Apple's
open-source releases. Without it the SDK compiles C but not C++: libc++'s
own `math.h` wraps the C library's, and there is nothing to wrap.

So this one header is vendored rather than built. It is the file macOS's
own libm descends from: Apple's implementation began as this code, which
is why the interface it declares is the one the platform provides.

* Source: `lib/msun/src/math.h` in <https://github.com/freebsd/freebsd-src>
* License: SunPro (permissive, notice-preserving) — see the file's header
* Vendored: 2026-09, from `main`

It is a header only. Nothing here is compiled: the implementations come
from the system's libm at link time, through the `libSystem` stub. This
declares the interface so a compiler can see it.

Update it by replacing the file from the same path upstream. It has no
local modifications, deliberately — a vendored file that has been edited
is one nobody can update.
