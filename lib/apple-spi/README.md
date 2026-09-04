# apple-spi

Reconstructed SPI headers that belong in `MacOSX.Internal.sdk` and
nowhere else.

`lib/libc-extra` is for headers Apple ships in the public SDK that
their open-source drops leave out. This is the other case: interfaces
Apple's own projects build against and no SDK publishes. They are
installed only into the internal SDK, which is the same rule the xnu
SPI under `usr/local/include` follows.

## Contents

- `msgtracer_client.h`, `msgtracer_keys.h` — see the headers. Apple's
  perl includes them and calls nothing in them; MessageTracer is gone
  from macOS.
