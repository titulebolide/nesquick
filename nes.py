import emu6502
import lstdebug
import inesparser
import ppu

# l = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")


    
class CartridgeRomDevice():
    def __init__(self, romfile):
        prg, chr = inesparser.parse_ines(romfile)
        self.mem = prg
    
    def __getitem__(self, key):
        return self.mem[key]
    
    def tick(self):
        pass
        

dev = ppu.PpuApuIODevice()
rom = CartridgeRomDevice("rom/hello-world/build/starter.nes")

mmap = [
    (0x0000, [0] * 0x800), # volontarily make it shorter to test for mirrors
    (0x2000, dev),
    (0X8000, rom),
]

# lst = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")

emu = emu6502.Emu6502(mmap, debug=True)

while True:
    emu.tick()
    dev.tick()
