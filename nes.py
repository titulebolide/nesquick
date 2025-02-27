import emu6502
import lstdebug
import inesparser
import ppu
import time
import multiprocessing

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


prg, chr = inesparser.parse_ines("rom/Donkey-Kong-NES-Disassembly/dk.nes")
lst = lstdebug.LstDebuggerAsm6("rom/Donkey-Kong-NES-Disassembly/dk.lst")

keyboard = ppu.KbDevice()

render_queue = multiprocessing.Queue()
renderer = ppu.PpuRenderer(chr, render_queue)

dev = ppu.PpuApuIODevice(chr, render_queue, keyboard.asciival)
rom = CartridgeRomDevice(prg)
ram = RamDevice()

mmap = [
    (0x0000, ram), # volontarily make it shorter to test for mirrors
    (0x2000, dev),
    (0Xc000, rom),
]

# lst = lstdebug.LstDebugger("rom/hello-world/build/starter.lst")
emu = emu6502.Emu6502(mmap, lst, debug=False)


dev.set_cpu_interrupt(emu.interrupt)
dev.set_cpu_ram(ram)



def main():
    tcpu = 0
    tgpu = 0
    n = 0
    while True:
        n+=1
        t1 = time.time()
        emu.tick()
        t2 = time.time()
        ## PPU runs at 3x PCU clock rate
        dev.tick()
        dev.tick()
        dev.tick()
        t3 = time.time()
        tcpu += t2-t1
        tgpu += t3-t2
        if n %100000 == 0:
            print("cpu %", round(tcpu/(tcpu+tgpu), 2), "kcycle/s", round(n/(tcpu+tgpu)/1e3, 2))
            tcpu = 0
            tgpu = 0
            n = 0

proc = multiprocessing.Process(target=main)
proc.start()
keyboard.start()
renderer.run()
