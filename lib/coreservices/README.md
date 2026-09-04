# coreservices

Reconstructed headers for `CoreServices.framework`.

CoreServices is not open source, and neither is any of the nine
subframeworks it umbrellas. What is here is written from the
documented API, in the same spirit as `lib/xpc`: the SDK needs
headers to compile against, and the framework binary it links to is
the running system's, stubbed as a `.tbd` from the shared cache.

## What is covered

`FSEvents` only. It is what git's `fsmonitor` backend uses on Darwin
(`compat/fsmonitor/fsm-listen-darwin.c`), which is what prompted it.
The other eight subframeworks are absent, and `CoreServices.h`
deliberately does not pretend otherwise.

## On the constant values

They were not written from memory or copied from Apple's header.
A program was compiled against the real framework, printed every
constant, and the printed values were pasted in — so they match the
system this builds on rather than someone's recollection. The
function list is exactly the 22 symbols `FSEvents.framework`
exports, taken from the generated `.tbd`.

## Licence

BSD-3-Clause, like the rest of this tree.
