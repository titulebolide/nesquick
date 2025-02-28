import emu6502
import lstdebug
import inesparser
import ppu
import time
import multiprocessing
import pickle


statefile = None #"state.bin"
statedata = None
if statefile is not None:
    with open(statefile, "rb") as f:
        statedata = pickle.load(f)


emu_state = ["regs", "stack_ptr", "prgm_ctr", "interrupt_type", "instruction_nbcycles", "instruction_cycle"]
ppu_state = ["vram", "ntick", "ppu_reg_w", "ppuaddr", "ppuctrl", "ppustatus", "ppuoam", "ppudata_buffer", "controller_strobe", "controller_read_no"]
ram_state = ["mem"]

def get_state(obj, statelist):
    return {i: getattr(obj, i) for i in statelist}

def set_state(obj, state):
    for key, val in state.items():
        setattr(obj, key, val)

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
emu = emu6502.Emu6502(mmap, lst, False)

dev.set_cpu_interrupt(emu.interrupt)
dev.set_cpu_ram(ram)

if statedata is not None:
    set_state(dev, statedata["ppu"])
    set_state(emu, statedata["emu"])
    set_state(ram, statedata["ram"])

class MainRunner(multiprocessing.Process):
    def __init__(self):
        super().__init__()
        self.done = False
    
    def stop(self):
        self.done = True

    def run_base(self):
        self.done = False
        tcpu = 0
        tgpu = 0
        n = 0
        while not self.done:
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

    def run(self):
        try:
            self.run_base()
        except KeyboardInterrupt:
            pass
        print("saving state")
        state = {
            "emu" : get_state(emu, emu_state),
            "ppu" : get_state(dev, ppu_state),
            "ram" : get_state(ram, ram_state),
        }

        with open("state.bin", "wb") as f:
            pickle.dump(state, f)

proc = MainRunner()
proc.start()
keyboard.start()
try:
    renderer.run()
except KeyboardInterrupt:
    pass
keyboard.stop()
proc.stop()
print("wait")
proc.join()
print("proc joined")
keyboard.join()
print("kb joined")
