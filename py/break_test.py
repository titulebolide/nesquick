from emu6502 import Emu6502
from lstdebug import LstDebugger

import rom

l = LstDebugger("break-tst/test.lst")

emu = Emu6502(l)

r = rom.Rom(emu, rom.mems_from_file("break-tst/test.bin", 0x8000))

emu.start()
print("running")
while True:
    input()
    emu.interrupt(maskable=True)
    print("interrupted")
