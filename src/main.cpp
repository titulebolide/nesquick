#include <iostream>
#include <chrono>

#include "lstdebugger.hpp"
#include "cpu.hpp"
#include "utils.hpp"
#include "device.hpp"
#include "ppu.hpp"


int main() {

    uint8_t prg[0x8000] = {0};
    uint8_t chr[0x4000] = {0}; // TODO : check sizes

    parseInes("../rom/Donkey-Kong-NES-Disassembly/dk.nes", prg, chr);

    // for (uint16_t addr = 0; addr < 0x8000; addr ++) {
    //     std::cout << prg[addr] << " ";
    // }

    LstDebuggerAsm6 lst("../rom/Donkey-Kong-NES-Disassembly/dk.lst");

    // keyboard = ppu.KbDevice()

    // render_queue = multiprocessing.Queue()
    // renderer = ppu.PpuRenderer(chr, render_queue)

    // dev = ppu.PpuApuIODevice(chr, render_queue, keyboard.state)
    CartridgeRomDevice rom(prg);
    RamDevice ram;
    PpuDevice ppu(chr, &ram);

    Memory mem({
        {0x0000, &ram},
        {0x2000, &ppu},
        {0xc000, &rom},
    });

    int16_t a = mem.get(0xfffc);

    std::cout << prg[1] << std::endl;

    std::cout << hexstr(mem.get(0xfffd)) << std::endl;
    // return 0;
    
    Emu6502 cpu(&mem, true, &lst);
    ppu.set_cpu(&cpu); // urgh

    // dev.set_cpu_interrupt(emu.interrupt)
    // dev.set_cpu_ram(ram)

    // if statedata is not None:
    //     set_state(dev, statedata["ppu"])
    //     set_state(emu, statedata["emu"])
    //     set_state(ram, statedata["ram"])

    // class MainRunner(multiprocessing.Process):
    //     def __init__(self):
    //         super().__init__()
    //         self.done = False
        
    //     def stop(self):
    //         self.done = True

    //     def run_base(self):
    //         self.done = False
    //         tcpu = 0
    //         tgpu = 0
    //         n = 0
    //         while not self.done:
    //             n+=1
    //             t1 = time.time()
    //             emu.tick()
    //             t2 = time.time()
    //             ## PPU runs at 3x PCU clock rate
    //             dev.tick()
    //             dev.tick()
    //             dev.tick()
    //             t3 = time.time()
    //             tcpu += t2-t1
    //             tgpu += t3-t2
    //             if n %100000 == 0:
    //                 print("cpu %", round(tcpu/(tcpu+tgpu), 2), "kcycle/s", round(n/(tcpu+tgpu)/1e3, 2))
    //                 tcpu = 0
    //                 tgpu = 0
    //                 n = 0

    //     def run(self):
    //         try:
    //             self.run_base()
    //         except KeyboardInterrupt:
    //             pass
    //         print("saving state")
    //         state = {
    //             "emu" : get_state(emu, emu_state),
    //             "ppu" : get_state(dev, ppu_state),
    //             "ram" : get_state(ram, ram_state),
    //         }

    //         with open("state.bin", "wb") as f:
    //             pickle.dump(state, f)

    // proc = MainRunner()
    // proc.start()
    // keyboard.start()
    // try:
    //     renderer.run()
    // except KeyboardInterrupt:
    //     pass
    // keyboard.stop()
    // proc.stop()
    // print("wait")
    // proc.join()
    // print("proc joined")
    // keyboard.join()
    // print("kb joined")

    unsigned long long loopCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        cpu.tick();
        ppu.tick();
        ppu.tick();
        ppu.tick();
        loopCount++;

        if (loopCount % 1000000 == 0) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - startTime;
            double loopsPerSecond = loopCount / elapsed.count();

            std::cout << "Loops per second: " << loopsPerSecond << std::endl;

            // Reset the counter and timer for the next million loops
            loopCount = 0;
            startTime = currentTime;
        }
    }


    return 0;
}
