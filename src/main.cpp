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
    LstDebuggerAsm6 lst("../rom/Donkey-Kong-NES-Disassembly/dk.lst", true);

    // parseInes("../rom/hello-world/build/starter.nes", prg, chr);
    // LstDebuggerAsm6 lst("../rom/hello-world/build/starter.lst", false);


    CartridgeRomDevice rom(prg);
    RamDevice ram;
    PpuDevice ppu(chr, &ram);

    Memory mem({
        {0x0000, &ram},
        {0x2000, &ppu},
        {0xc000, &rom},
    });

    Emu6502 cpu(&mem, false, &lst);
    ppu.set_cpu(&cpu); // urgh


    unsigned long long loopCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        cpu.tick();
        ppu.tick();
        ppu.tick();
        ppu.tick();
        loopCount++;

        if (loopCount % 1000000 == 0) {
            ppu.render();
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
