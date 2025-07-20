#pragma once

#include <opencv2/opencv.hpp> 

#include "device.hpp"
#include "cpu.hpp"
#include <string>


enum {
	KEY_PPUCTRL = 0x2000,
	KEY_PPUMASK = 0x2001,
	KEY_PPUSTATUS = 0x2002,
	KEY_OAMADDR = 0x2003,
	KEY_OAMDATA = 0x2004,
	KEY_PPUSCROLL = 0x2005,
	KEY_PPUADDR = 0x2006,
	KEY_PPUDATA = 0x2007,
	KEY_OAMDMA = 0x4014,
    KEY_APU_STATUS = 0x4015,
	KEY_CTRL1 = 0x4016,
    KEY_CTRL2 = 0x4017,
};


static const uint8_t PPUCTRL_VBLANKNMI     = 0b10000000; // 0 : disable NMI on vblank
static const uint8_t PPUCTRL_SPRITESIZE    = 0b00100000; // 0 : 8x8, 1: 8x16
static const uint8_t PPUCTRL_BGPATTTABLE   = 0b00010000; // Pattern table no select in 8x8 mode
static const uint8_t PPUCTRL_OAMPATTTABLE  = 0b00001000; // Pattern table no select in 8x8 mode
static const uint8_t PPUCTRL_VRAMINC       = 0b00000100;

static const uint8_t PPUOAM_ATT_HFLIP = 0b01000000;
static const uint8_t PPUOAM_ATT_VFLIP = 0b10000000;

static const uint8_t PPUSTATUS_SPRITE0_COLLISION = BIT6;

const uint8_t NES_COLORS[64][3] = {{124, 124, 124}, {0, 0, 252}, {0, 0, 188}, {68, 40, 188}, {148, 0, 132}, {168, 0, 32}, {168, 16, 0}, {136, 20, 0}, {80, 48, 0}, {0, 120, 0}, {0, 104, 0}, {0, 88, 0}, {0, 64, 88}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {188, 188, 188}, {0, 120, 248}, {0, 88, 248}, {104, 68, 252}, {216, 0, 204}, {228, 0, 88}, {248, 56, 0}, {228, 92, 16}, {172, 124, 0}, {0, 184, 0}, {0, 168, 0}, {0, 168, 68}, {0, 136, 136}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {248, 248, 248}, {60, 188, 252}, {104, 136, 252}, {152, 120, 248}, {248, 120, 248}, {248, 88, 152}, {248, 120, 88}, {252, 160, 68}, {248, 184, 0}, {184, 248, 24}, {88, 216, 84}, {88, 248, 152}, {0, 232, 216}, {120, 120, 120}, {0, 0, 0}, {0, 0, 0}, {252, 252, 252}, {164, 228, 252}, {184, 184, 248}, {216, 184, 248}, {248, 184, 248}, {248, 164, 192}, {240, 208, 176}, {252, 224, 168}, {248, 216, 120}, {216, 248, 120}, {184, 248, 184}, {184, 248, 216}, {0, 252, 252}, {248, 216, 248}, {0, 0, 0}, {0, 0, 0}};

const uint16_t SCANLINE_LENGHT = 341;
const uint16_t SCANLINE_NUMBER = 262;
const uint16_t SCANLINE_VBLANK_START = 241;
const uint16_t SCANLINE_FLAG_CLEAR = 261;

class PpuDevice : public Device {
private:
    uint8_t m_chr_rom[0x4000];

    // TODO : this is quite bad, we share here cpuram for OAMDMA
    Device * m_cpu_ram;
    // Same, needed to forward 4017 writes...
    Device * m_apu;
    // this is used to call the interrupt, same, could do better (interface ?)
    Emu6502 * m_cpu;

    // cpu_interrupt = None
    // cpu_ram = None

    uint8_t m_vram[0x4000] = {0}; // 14 bit addr space
    uint32_t m_ntick = 0;
    uint8_t m_ppu_reg_w = 0;
    uint16_t m_ppuaddr = 0;
    uint8_t m_ppuctrl = 0;
    uint8_t m_ppustatus = 0;
    uint8_t m_ppuoam[256] = {0};
    uint8_t m_ppudata_buffer = 0; // ppudata does not read directly ram but a buffer that is updated after each read

    uint8_t m_ppuscroll_x = 0;
    uint8_t m_ppuscroll_y = 0;

    uint8_t m_controller_strobe = 0;
    uint8_t m_controller_read_no = 0;

    uint8_t m_kb_state = 0;
    
    cv::Mat m_next_frame; // frame that we are building
    cv::Mat m_last_frame; // last frame that we built

    std::string m_dbg_string;

    
    bool get_ppuctrl_bit(uint8_t status_bit);

    void inc_ppuaddr();
    bool add_sprite(cv::Mat *frame, uint8_t sprite_no, bool table_no, uint16_t sprite_x, uint16_t sprite_y, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision);
    bool add_sprite_line(uint8_t sprite_no, bool table_no, uint8_t sprite_x, uint8_t sprite_y, uint8_t sprite_line, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision);
    void get_sprite(uint8_t sprite[8][8], uint8_t sprite_no, bool table_no, bool doubletile);

    void get_sprite_line(uint8_t sprite[8], uint8_t sprite_no, bool table_no, uint8_t sprite_line, bool hflip, bool vflip);
    
    
public:
    void render_oam();
    void render_oam_line(uint8_t line_no);
    void render_nametable_line(uint8_t screen_sprite_y);
    void dbg_render_fullnametable(cv::Mat *dbg_frame);
    PpuDevice(uint8_t *chr_rom, Device *cpu_ram, Device *apu);
    uint8_t get(uint16_t addr);
    void set(uint16_t addr, uint8_t val);
    void tick();
    void set_cpu(Emu6502 *cpu);
    void set_kb_state(uint8_t kb_state);
    void render();
    cv::Mat *getFrame();
    void saveFrame();
};
