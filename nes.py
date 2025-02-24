import emu6502
import lstdebug
import inesparser
import ppu

# l = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")


    
class CartridgeRomDevice():
    def __init__(self, prg_rom):
        self.mem = prg_rom
    
    def __getitem__(self, key):
        return self.mem[key]
    
    def tick(self):
        pass
        
class RamDevice():
    def __init__(self):
        self.mem = [0] * 0x800
    
    def __getitem__(self, key):
        return self.mem[key]
    
    def __setitem__(self, key, val):
        self.mem[key] = val


prg, chr = inesparser.parse_ines("rom/hello-world/build/starter.nes")

dev = ppu.PpuApuIODevice(chr)
rom = CartridgeRomDevice(prg)
ram = RamDevice()

mmap = [
    (0x0000, ram), # volontarily make it shorter to test for mirrors
    (0x2000, dev),
    (0X8000, rom),
]

# lst = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")

emu = emu6502.Emu6502(mmap, debug=True)

dev.set_cpu_interrupt(emu.interrupt)
dev.set_cpu_ram(ram)

while True:
    emu.tick()
    dev.tick()
