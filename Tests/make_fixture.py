#!/usr/bin/env python3
"""Original Nintendo-free GBC test program: fill a tile and loop at VBlank.
No game graphics, boot ROM or Nintendo logo are used.
"""
from pathlib import Path
import sys

def make_rom():
    rom = bytearray(32768)
    rom[0x100:0x104] = bytes([0x00, 0xc3, 0x50, 0x01])
    rom[0x134:0x13f] = b"DXIOS TEST!"
    rom[0x143] = 0x80
    rom[0x147] = 0
    rom[0x148] = 0
    # DI; SP=$fffe; LCD off; B=16; HL=$8000; fill tile with alternating pixels;
    # LCD on; repeatedly read LY, then increment WRAM $c000 each VBlank.
    code = bytes.fromhex("f3 31 fe ff af e0 40 06 10 21 00 80 3e aa 22 05 20 fa 3e 91 e0 40 f0 44 fe 90 20 fa 21 00 c0 34 f0 44 fe 90 28 fa 18 ec")
    rom[0x150:0x150 + len(code)] = code
    check = 0
    for byte in rom[0x134:0x14d]:
        check = (check - byte - 1) & 255
    rom[0x14d] = check
    total = sum(rom) & 65535
    rom[0x14e:0x150] = total.to_bytes(2, "big")
    return bytes(rom)

if __name__ == "__main__":
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(make_rom())
