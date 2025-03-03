#pragma once

#include "device.hpp"
#include "cpu.hpp"

enum {
	KEY_PPUCTRL = 0,
	KEY_PPUMASK = 1,
	KEY_PPUSTATUS = 2,
	KEY_OAMADDR = 3,
	KEY_OAMDATA = 4,
	KEY_PPUSCROLL = 5,
	KEY_PPUADDR = 6,
	KEY_PPUDATA = 7,
	KEY_OAMDMA = 0x2014,
	KEY_CTRL1 = 0x2016,
    KEY_CTRL2 = 0x2017,
};


static const uint16_t PPUCTRL_VBLANKNMI     = 0b10000000; // 0 : disable NMI on vblank
static const uint16_t PPUCTRL_SPRITESIZE    = 0b00100000; // 0 : 8x8, 1: 8x16
static const uint16_t PPUCTRL_BGPATTTABLE   = 0b00010000; // Pattern table no select in 8x8 mode
static const uint16_t PPUCTRL_OAMPATTTABLE  = 0b00001000; // Pattern table no select in 8x8 mode
static const uint16_t PPUCTRL_VRAMINC       = 0b00000100;

static const uint16_t PPUOAM_ATT_HFLIP = 0b01000000;
static const uint16_t PPUOAM_ATT_VFLIP = 0b10000000;

class PpuDevice : public Device {
private:
    uint8_t chr_rom[0x4000];

    // TODO : this is quite bad, we share here cpuram for OAMDMA
    Device * cpu_ram;
    // this is used to call the interrupt, same, could do better (interface ?)
    Emu6502 * cpu;

    // cpu_interrupt = None
    // cpu_ram = None

    uint8_t vram[0x4000] = {0}; // 14 bit addr space
    long ntick = 0;
    uint8_t ppu_reg_w = 0;
    uint8_t ppuaddr = 0;
    uint8_t ppuctrl = 0;
    uint8_t ppustatus = 0;
    uint8_t ppuoam[256] = {0};
    uint8_t ppudata_buffer = 0; // ppudata does not read directly ram but a buffer that is updated after each read

    uint8_t controller_strobe = 0;
    uint8_t controller_read_no = 0;
    
    bool get_ppuctrl_bit(uint8_t status_bit);

    void inc_ppuaddr();


public:
    PpuDevice(uint8_t * chr_rom, Device * cpu_ram);
    uint8_t get(uint16_t addr);
    void set(uint16_t addr, uint8_t val);
    void tick();
    void set_cpu(Emu6502 * cpu);
};
