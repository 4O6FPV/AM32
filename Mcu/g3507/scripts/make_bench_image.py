#!/usr/bin/env python3
"""
Build a bench image for an MSPM0G3507 that has no AM32 bootloader yet.

The AM32 application is linked at 0x1000. Until the AM32 bootloader is
ported, the part needs something at 0x0 to boot it. A Cortex-M0+ only needs
two words there: the initial stack pointer and the reset handler address, so
this copies the application's own first two vector entries to 0x0 and pads
the rest of the bootloader area with 0xFF. The application then moves VTOR
to 0x1000 itself (initAfterJump()).

Usage:
    python3 Mcu/g3507/scripts/make_bench_image.py obj/AM32_NDAA_REF_G3507_2.21.bin out.bin

Flash out.bin at address 0x00000000 (it covers 0x0 up to the end of the app).
Rebuild the image every time the firmware is rebuilt: the reset handler
address moves.
"""

import struct
import sys

APP_OFFSET = 0x1000


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    app = open(sys.argv[1], "rb").read()
    sp, reset = struct.unpack_from("<II", app, 0)
    if not (0x20200000 < sp <= 0x20208000):
        print(f"unexpected initial SP 0x{sp:08x}; is this the G3507 app .bin?")
        return 1
    if not (APP_OFFSET <= (reset & ~1) < APP_OFFSET + len(app)):
        print(f"reset handler 0x{reset:08x} is not inside the app image")
        return 1
    stub = struct.pack("<II", sp, reset)
    image = stub + b"\xff" * (APP_OFFSET - len(stub)) + app
    open(sys.argv[2], "wb").write(image)
    print(f"SP=0x{sp:08x} reset=0x{reset:08x}, wrote {len(image)} bytes for address 0x0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
