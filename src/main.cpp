#include <iostream>
#include <chrono>
#include "audio.hpp"


#include "lstdebugger.hpp"
#include "cpu.hpp"
#include "utils.hpp"
#include "device.hpp"
#include "ppu.hpp"
#include "apu.hpp"

#include <opencv2/opencv.hpp>
#include <SDL.h>

#include <iostream>
#include <thread>

#include <signal.h>
#include <map>

#define DEBUG_WINDOW false

typedef std::chrono::high_resolution_clock Clock;

static int const NSTEPS_PAUSE = 10000;
static long const TIME_BETWEEN_PAUSE_US = (double)NSTEPS_PAUSE * 1000000.0f /(double)CLOCK_FREQUENCY * 2;

std::map<char,uint8_t> CONTROLLER_MAPPING = {{'p', 0}, {'o', 1}, {'b', 2}, {'n', 3}, {'z', 4}, {'s', 5}, {'q', 6}, {'d', 7}}; // A, B, Select, Start, Up, Down, Left, Right

void turn_bit_off(uint8_t * value, uint8_t bit) {
    *value &= ~(1 << bit);
}

void turn_bit_on(uint8_t * value, uint8_t bit) {
    *value |= (1 << bit);
}


void ui(Emu6502 * cpu, PpuDevice * ppu, ApuDevice * apu) {
    
    // init SDL
    struct sigaction action;
    sigaction(SIGINT, NULL, &action);
    SDL_Init(SDL_INIT_EVERYTHING);
    if (SDL_Init(SDL_INIT_NOPARACHUTE) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return;
    }
    sigaction(SIGINT, &action, NULL);


    apu->start_sound();


    if(!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0"))
    {
        std::cout << "SDL can not disable compositor bypass!" << std::endl;
        return;
    }

    SDL_Window* window = SDL_CreateWindow("Display Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64*8*2, 60*8*2, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_INPUT_FOCUS);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    SDL_Texture * texture = nullptr;
    if (DEBUG_WINDOW) {
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 64*8, 60*8);
    } else {
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 64*4, 60*4);
    }
    
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    cv::Mat * frame = ppu->getFrame();
    cv::Mat dbg_frame(64*8, 64*8, CV_8UC3);
    
    bool thread_done = false;

    uint8_t kb_state = 0;

    while(!thread_done) {
        SDL_Event e;
        uint8_t kb_status = 0;
        // Wait indefinitely for the next available event
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                thread_done = true;
            }
            if (e.type == SDL_KEYDOWN | e.type == SDL_KEYUP) {
                uint8_t keycode = 0;
                try {
                    keycode = CONTROLLER_MAPPING.at(e.key.keysym.sym);
                } catch(const std::out_of_range& ex) {
                    if (e.key.keysym.sym == 'g') {
                        cpu->setDebug(true);
                    }
                    continue;
                }
                if (e.type == SDL_KEYDOWN) {
                    turn_bit_on(&kb_state, keycode);
                } else {
                    turn_bit_off(&kb_state, keycode);
                }
            }
        }

        ppu->set_kb_state(kb_state);

        if (DEBUG_WINDOW) {
          ppu->dbg_render_fullnametable(&dbg_frame);

          // Define the ROI in the larger frame where the smaller frame will be placed
          cv::Rect roi(0, 240, 256, 240);

          // Copy the smaller frame into the ROI of the larger frame
          frame->copyTo(dbg_frame(roi));

          SDL_UpdateTexture(texture, nullptr, dbg_frame.data, dbg_frame.step1());
        } else {
          SDL_UpdateTexture(texture, nullptr, frame->data, frame->step1());
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void run(Emu6502 * cpu, PpuDevice * ppu, ApuDevice * apu, bool * thread_done) {
    unsigned long long loopCount = 0;
    auto last_t = Clock::now();
    float load_sum = 0.0f;
    int load_num = 0;
    while (!(*thread_done)) {
        cpu->tick();

        ppu->tick();
        ppu->tick();
        ppu->tick();

        cpu->tick();

        ppu->tick();
        ppu->tick();
        ppu->tick();

        apu->tick();
    
        loopCount++;

        if (loopCount % NSTEPS_PAUSE == 0) {
            auto now = Clock::now();
            // slow down !
            loopCount = 0;
            long elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(now - last_t).count(); 

            // evaluating cpu load
            load_sum += (float)elapsed_time / (float)TIME_BETWEEN_PAUSE_US;
            load_num++;
            if (load_num == 100) {
                std::cout << "Load : " << load_sum << "% " << TIME_BETWEEN_PAUSE_US << "µs" << std::endl;
                load_sum = 0.0f;
                load_num = 0;
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(TIME_BETWEEN_PAUSE_US - elapsed_time));
            last_t = Clock::now();
        }
    }
}

int main() {
    uint8_t prg[0x8000] = {0};
    uint8_t chr[0x4000] = {0}; // TODO : check sizes
    uint16_t prgLen, chrLen;

    // parseInes("../rom/Donkey-Kong-NES-Disassembly/dk.nes", prg, chr, &prgLen, &chrLen);
    // LstDebuggerAsm6 lst("../rom/Donkey-Kong-NES-Disassembly/dk.lst", true);

    parseInes("/home/titus/dev/nesquick/rom/smb1/bin/smb1.nes", prg, chr, &prgLen, &chrLen);
    LstDebuggerAsm6 lst("/home/titus/dev/nesquick/rom/smb1/bin/smb1.lst", true);

    // parseInes("/home/titus/dev/nesquick/rom/pacman.nes", prg, chr, &prgLen, &chrLen);


    uint16_t rom_base_addr = 0x10000 - prgLen;

    CartridgeRomDevice rom(prg, rom_base_addr);
    RamDevice ram(0x0000);
    ApuDevice apu;
    PpuDevice ppu(chr, &ram, &apu);

    Memory mem({
        {0x0000, &ram},
        {0x2000, &ppu},
        {0x4000, &apu},
        {0x4014, &ppu},
        {rom_base_addr, &rom},
    });

    Emu6502 cpu(&mem, false, &lst);
    ppu.set_cpu(&cpu); // urgh
    apu.set_cpu(&cpu); // urgh


    bool kill = false;
    std::thread t1(run, &cpu, &ppu, &apu, &kill); 

    ui(&cpu, &ppu, &apu);

    kill = true;

    t1.join();
    
    return 0;
}
