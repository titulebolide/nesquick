#include <iostream>
#include <chrono>

#include "lstdebugger.hpp"
#include "cpu.hpp"
#include "utils.hpp"
#include "device.hpp"
#include "ppu.hpp"

#include <opencv2/opencv.hpp>
#include <SDL2/SDL.h>
#include <iostream>



int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Display Image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }


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

    cv::Mat * frame = ppu.getFrame();

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 30*8, 32*8);
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    while (true) {
        cpu.tick();
        ppu.tick();
        ppu.tick();
        ppu.tick();
        loopCount++;

        
        if (loopCount % 200000 == 0) {
            ppu.render();
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - startTime;
            double loopsPerSecond = loopCount / elapsed.count();

            std::cout << "Loops per second: " << loopsPerSecond << std::endl;

            // Reset the counter and timer for the next million loops
            loopCount = 0;
            startTime = currentTime;

        
            SDL_UpdateTexture(texture, nullptr, frame->data, frame->step1());
        
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    }

    // SDL_Delay(5000);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
