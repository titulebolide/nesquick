#include <cstdint>
#include "ppu.hpp"
#include "utils.hpp"

PpuDevice::PpuDevice(uint8_t * _chr_rom, Device * cpu_ram, Device * apu) : 
    m_cpu_ram(cpu_ram), m_cpu(nullptr), m_apu(apu), m_last_frame(30*8, 32*8, CV_8UC3), m_next_frame(30*8, 32*8, CV_8UC3) {

    for (uint16_t addr = 0; addr < 0x4000; addr ++) {
        m_chr_rom[addr] = _chr_rom[addr];
    }
}

void PpuDevice::set_cpu(Emu6502 *_cpu) {
    m_cpu = _cpu;
}

void PpuDevice::set_kb_state(uint8_t kb_state) {
    m_kb_state = kb_state;
}

bool PpuDevice::get_ppuctrl_bit(uint8_t status_bit) {
    return ((m_ppuctrl & status_bit) != 0);
}

void PpuDevice::inc_ppuaddr() {
    /*
    Outside of rendering, reads from or writes to $2007 will add either 1 or 32 
    to v depending on the VRAM increment bit set via $2000. During rendering (on
    the pre-render line and the visible lines 0-239, provided either background 
    or sprite rendering is enabled), it will update v in an odd way, triggering 
    a coarse X increment and a Y increment simultaneously
    */
    uint16_t scanline_no = m_ntick / SCANLINE_LENGHT;
    if (SCANLINE_LAST_VISIBLE < scanline_no && scanline_no < SCANLINE_PRE_RENDER) {
        // outside rendering
        // quite normal ppu addr incr
        if (get_ppuctrl_bit(PPUCTRL_VRAMINC)) {
            m_ppuaddr += 32;
            m_ppu_reg_v += 32;
        } else {
            m_ppuaddr += 1;
            m_ppu_reg_v += 1;
        }
    } else {
        // TODO : check rendering is enabled
        coarse_x_incr();
        y_incr();
    }
}

void PpuDevice::coarse_x_incr() {
    if ((m_ppu_reg_v & 0x001F) == 31) {
        // if coarse X == 31
        m_ppu_reg_v &= ~0x001F;          // coarse X = 0
        m_ppu_reg_v ^= 0x0400;           // switch horizontal nametable
    } else {
        m_ppu_reg_v += 1;
    }
}

void PpuDevice::y_incr() {
    if ((m_ppu_reg_v & 0x7000) != 0x7000) {
        // if fine Y < 7
        m_ppu_reg_v += 0x1000;                      // increment fine Y
    } else {
        m_ppu_reg_v &= ~0x7000;                     // fine Y = 0
        uint16_t y = (m_ppu_reg_v & 0x03E0) >> 5;        // let y = coarse Y
        if (y == 29) {
            y = 0;                          // coarse Y = 0
            m_ppu_reg_v ^= 0x0800;                    // switch vertical nametable
        } else if (y == 31) {
            y = 0;                          // coarse Y = 0, nametable not switched
        } else {
            y += 1;                         // increment coarse Y
        }
        m_ppu_reg_v = (m_ppu_reg_v & ~0x03E0) | (y << 5);     // put coarse Y back into v
    }
}

// https://www.nesdev.org/wiki/PPU_scrolling
void PpuDevice::set(uint16_t addr, uint8_t value) {
    uint16_t value16b = static_cast<uint16_t>(value);
    if (addr < 0x4000) {
        m_last_bus_value = value;
        addr = ((addr - 0x2000) % 8) + 0x2000; // mirroring every 8 bits
    }
    uint16_t oamdma_source_addr;
    switch (addr)
    {
    case KEY_PPUCTRL:
        // t: ...GH.. ........ <- d: ......GH
        //    <used elsewhere> <- d: ABCDEF..
        
        if ((m_ppuctrl & 0b11) != (value & 0b11) ) {

            uint16_t scanline_no = m_ntick / SCANLINE_LENGHT;
            uint16_t column_no = m_ntick % SCANLINE_LENGHT;
            // std::cout << "nametable set to " << (int) (value & 0b11) << " frame " << m_n_frame << " scanline " << (int) scanline_no << " column " << (int) column_no << std::endl;
            // m_cpu->dbg();

            // sleep(2);
        }
        m_ppuctrl = value;

        clear_bits(&m_ppu_reg_t, BIT10|BIT11);
        m_ppu_reg_t |= (value & 0b11) << 10;
        break;
    
    case KEY_PPUMASK:
        m_ppumask = value;
        break;
    
    case KEY_PPUADDR:
        // done in two reads : msb, then lsb
        if (m_ppu_reg_w == 0) {
            // t: .CDEFGH ........ <- d: ..CDEFGH
            //        <unused>     <- d: AB......
            // t: Z...... ........ <- 0 (bit Z is cleared)
            // w:                  <- 1

            // msb, null the most signifants two bits (14 bit long addr space)
            m_ppuaddr = value16b & 0b00111111;

            // only bits 8->14 are set by value but bit 15 is also cleared 
            clear_bits(&m_ppu_reg_t, 0b1111111100000000);
            m_ppu_reg_t |= (value16b & 0b00111111) << 8;

        } else {
            // t: ....... ABCDEFGH <- d: ABCDEFGH
            // v: <...all bits...> <- t: <...all bits...>
            // w:                  <- 0

            //we are reading the lsb
            m_ppuaddr = (m_ppuaddr << 8) + value16b;

            clear_bits(&m_ppu_reg_t, 0b11111111);
            m_ppu_reg_t |= value16b;

            m_ppu_reg_v = m_ppu_reg_t;
        }
        // flip w
        m_ppu_reg_w = !m_ppu_reg_w;
        break;

    case KEY_PPUDATA:
        m_vram[m_ppuaddr] = value;
        inc_ppuaddr();
        break;

    case KEY_PPUSCROLL:
        if (m_ppu_reg_w == 0) {
            // t: ....... ...ABCDE <- d: ABCDE...
            // x:              FGH <- d: .....FGH
            // w:                  <- 1

            // 1st write, we are reading X
            m_ppuscroll_x = value16b;

            clear_bits(&m_ppu_reg_t, 0b11111);
            m_ppu_reg_t |= value16b >> 3;

            m_ppu_reg_x = value & 0b111;
        } else {
            // t: FGH..AB CDE..... <- d: ABCDEFGH
            // w:                  <- 0

            // 2nd write, we are reading Y
            m_ppuscroll_y = value;

            clear_bits(&m_ppu_reg_t, 0b0111001111100000);
            // set FGH
            m_ppu_reg_t |= (value16b & 0b111) << 12;
            // set ABCDE
            m_ppu_reg_t |= (value16b & 0b11111000) << 5;
        }
        // flip w
        m_ppu_reg_w = !m_ppu_reg_w;
        break;

    case KEY_OAMDMA:
        // todo : emulate cpu cycles for DMA ?
        oamdma_source_addr = (value16b << 8);
        for (uint16_t i = 0; i < 256; i ++) {
            m_ppuoam[i] = m_cpu_ram->get(oamdma_source_addr + i);
        }
        break;

    case KEY_OAMADDR:
        // oamdata is not implemented yet, so m_ppu_oam_addr is useless for now
        m_ppu_oam_addr = value;

    case KEY_CTRL1:
        m_controller_strobe = (value & 1); // get lsb
        if (m_controller_strobe == 1) {
            m_controller_read_no = 0;
        }
        break;

    case KEY_APU_STATUS:
        m_apu->set(addr, value);

    case KEY_CTRL2:
        // this write corresponds to the APU set mode and interrupt...
        m_apu->set(addr, value);

    default:
        // std::cout << "UNIMPLEMENETED " << hexstr(addr) << std::endl;
        break;
    }
}

uint8_t PpuDevice::get(uint16_t addr) {
    bool update_last_bus_value = false;
    if (addr < 0x4000) {
        update_last_bus_value = true;
        addr = ((addr - 0x2000) % 8) + 0x2000; // mirroring every 8 bits
    }

    uint8_t retval;
    uint8_t controller_state;
    switch (addr) {
    case KEY_PPUDATA:
        // get buffer value
        retval = m_ppudata_buffer;
        // update buffer AFTER the read
        if (m_ppuaddr < 0x2000) {
            m_ppudata_buffer = m_chr_rom[m_ppuaddr];
        } else if (m_ppuaddr < 0x3F00) {
            m_ppudata_buffer = m_vram[0x1000 + (m_ppuaddr-0x1000)%0x1000];
        } else if (m_ppuaddr < 0x4000) {
            m_ppudata_buffer = m_vram[0x1000 + (m_ppuaddr-0x1000)%0x1000];
        } else {
            std::cerr << "Invalid vram addr" << std::endl;
        }
        // increase ppuaddr after access
        inc_ppuaddr();
        break;
    
    case KEY_PPUSTATUS:
        // w:                  <- 0

        // keep only the three high bits
        // set the lower bits to the reminiscient of the bus (open bus)
        retval = (m_ppustatus & (BIT7|BIT6|BIT5)) | (m_last_bus_value & (BIT4|BIT3|BIT2|BIT1|BIT0));

        // reset w register (PUSTATUS read side effect)
        m_ppu_reg_w = 0;
        m_ppustatus &= byte_not(PPUSTATUS_VBLANK);
        break;
    
    case KEY_CTRL1:
        // TODO : In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. Usually this is the most significant byte of the address of the controller port—0x40. Certain games (such as Paperboy) rely on this behavior and require that reads from the controller ports return exactly $40 or $41 as appropriate. See: Controller reading: unconnected data lines.
        controller_state = m_kb_state; // TODO link to a keyboard this

        if (m_controller_read_no > 7) {
            retval = 1;
        }
        else {
            retval = ((controller_state >> m_controller_read_no) & 1);
            if (m_controller_strobe == 0) {
                m_controller_read_no += 1;
            }
        }
        break;
        
    case KEY_CTRL2:
        retval = 0x40;
        break;

    default:
        break;
    }
    if (update_last_bus_value) {
        m_last_bus_value = retval;
    }
    return retval;
}

// https://www.nesdev.org/w/images/default/4/4f/Ppu.svg
void PpuDevice::tick() {
    uint16_t scanline_no = m_ntick / SCANLINE_LENGHT;
    uint16_t column_no = m_ntick % SCANLINE_LENGHT;

    // TODO : turn this into a lookup table of pointer to exec function for matching columns
    if (column_no == 0) {
        // beginning of a new visible scanline
        if (scanline_no < SCANLINE_VBLANK_START) {
            // we are in a visible scanline
            // if (scanline_no%8 == 0) {
            //     // render background batch of 8 lines
            //     render_nametable_line(scanline_no / 8);
            // }
            render_oam_line(scanline_no);
        }
    } else if (column_no == 1) {
        if (scanline_no == SCANLINE_VBLANK_START) {
            // we are in the first tick of vblank
            // Let's finish rendering the frame
            // render_oam();
            saveFrame();
            m_ppustatus |= PPUSTATUS_VBLANK;

            // TODO : should not be byte_not a macro or something so it gets notted at compil and not runtime ?
            // TODO : check we are resetting SPRITE0 collision flag at the right moment
            if (get_ppuctrl_bit(PPUCTRL_VBLANKNMI)) {
                m_cpu->interrupt(false);
            }
        } else if (scanline_no == SCANLINE_PRE_RENDER) {
            // clear vblank and sprite 0 collision
            // TODO : clear also sprite overflow
            m_ppustatus &= byte_not(PPUSTATUS_OVERFLOW);
            m_ppustatus &= byte_not(PPUSTATUS_SPRITE0_COLLISION);
            m_ppustatus &= byte_not(PPUSTATUS_VBLANK);
        }
    } else if (8 <= column_no && column_no <= 248 && column_no%8 == 0) {
        if (scanline_no == SCANLINE_PRE_RENDER || scanline_no <= SCANLINE_LAST_VISIBLE) {
            if (scanline_no%8 == 7) {
                render_one_fucking_nametable_tile();
            }
            coarse_x_incr();
        }
    } else if (column_no == 256) {
        // https://www.nesdev.org/wiki/PPU_scrolling#At_dot_256_of_each_scanline
        if (scanline_no == SCANLINE_PRE_RENDER || scanline_no <= SCANLINE_LAST_VISIBLE) {
            // if (scanline_no%8 == 7) {
            //     render_one_fucking_nametable_tile();
            // }
            y_incr();
            coarse_x_incr();
        }
    } else if (column_no == 257) {
        // https://www.nesdev.org/wiki/PPU_scrolling#At_dot_257_of_each_scanline
        // v: ....A.. ...BCDEF <- t: ....A.. ...BCDEF
        m_ppu_reg_v |= m_ppu_reg_t & 0b0000010000011111;

    }  else if (column_no == 258) {
        if (scanline_no == SCANLINE_LAST_VISIBLE +1) {
            std::cout << (int) column_no << " " << (int) scanline_no << " " << (int)m_nt_tile_x << " " << (int)m_nt_tile_y << std::endl;
        }
    } else if (280 <= column_no && column_no <= 304) {
        // https://www.nesdev.org/wiki/PPU_scrolling#During_dots_280_to_304_of_the_pre-render_scanline_(end_of_vblank)
        if (scanline_no == SCANLINE_PRE_RENDER) {
            // v: GHIA.BC DEF..... <- t: GHIA.BC DEF.....
            m_ppu_reg_v |= m_ppu_reg_t & 0b0111101111100000;
        }
    } else if (column_no == 328) {
        if (scanline_no == SCANLINE_PRE_RENDER || scanline_no <= SCANLINE_LAST_VISIBLE) {
            // if (scanline_no%8 == 7) {
            //     render_one_fucking_nametable_tile();
            // }
            coarse_x_incr();
        }
    } else if (column_no == 336) {
        if (scanline_no == SCANLINE_PRE_RENDER || scanline_no <= SCANLINE_LAST_VISIBLE) {
            if (scanline_no%8 == 7) {
                render_one_fucking_nametable_tile();
            }
            coarse_x_incr();
        }
    } 
    m_ntick++;
    if (m_ntick == SCANLINE_NUMBER * SCANLINE_LENGHT) {
        m_ntick = 0;
        m_n_frame++;
    }
}

// renders the background
void PpuDevice::render_nametable_line(uint8_t screen_sprite_y) {
    // x is left to right
    // y is up to down
    // but for imshow x is up to down, y is left to right
    uint8_t y_shift = 0; // (m_ppuscroll_y+1)%8;
    uint8_t x_shift = m_ppuscroll_x % 8;
    for (int16_t screen_sprite_x = 0; screen_sprite_x < 32; screen_sprite_x++) {
        uint16_t sprite_x = screen_sprite_x + static_cast<uint16_t>(m_ppuscroll_x/8);
        uint16_t sprite_y = screen_sprite_y + static_cast<uint16_t>(m_ppuscroll_y/8);
        uint16_t nametable_no = m_ppuctrl & 0b11;  
        // if (screen_sprite_y <= 3) {
        //   nametable_no = 0;
        // }
        if (sprite_y >= 30) {
            // crossing vertically the nametables
            sprite_y -= 30;
            if (nametable_no <= 1) {
                // nametable 0 or 1, we go to 2 or 3
                nametable_no += 2;
            } else {
                // nametable 2 or 3, we go to 0 or 1
                nametable_no -= 2;
            }
        }
        if (sprite_x >= 32) {
            // crossing horizontally the nametables
            sprite_x -= 32;
            if (nametable_no%2==0) {
                // nametable 0 or 2, we go to 1 or 3
                nametable_no += 1;
            } else {
                // nametable 1 or 3, we go to 0 or 2
                nametable_no -= 1;
            }
        }
        uint16_t nametable_base_addr = 0x2000 + 0x400*nametable_no;
        uint8_t sprite_no = m_vram[nametable_base_addr + sprite_x + sprite_y * 32];
        /*
        7654 3210
        |||| ||++- Color bits 3-2 for top left quadrant of this byte
        |||| ++--- Color bits 3-2 for top right quadrant of this byte
        ||++------ Color bits 3-2 for bottom left quadrant of this byte
        ++-------- Color bits 3-2 for bottom right quadrant of this byte
        */
        uint16_t attribute_table_addr = 0x3c0 +  (static_cast<uint16_t>(sprite_y) / 4)*8 + static_cast<uint16_t>(sprite_x) / 4;
        uint8_t attr_bitshift = 0;
        if (sprite_y % 4 > 1) {
            // bottom
            attr_bitshift += 4;
        }
        if (sprite_x % 4 > 1) {
            // right
            attr_bitshift += 2;
        }
        uint8_t palette_no = ((m_vram[nametable_base_addr + attribute_table_addr] >> attr_bitshift) & 0b11);
        bool table_no = get_ppuctrl_bit(PPUCTRL_BGPATTTABLE);
        uint8_t actual_x_shift = 0;
        if (screen_sprite_x >= 1) {
          actual_x_shift = x_shift;
        }
        add_sprite(&m_next_frame, sprite_no, table_no, screen_sprite_x*8-actual_x_shift, screen_sprite_y*8-y_shift, palette_no, false, false, false);
    }
}

// renders the background
void PpuDevice::render_one_fucking_nametable_tile() {
    // x is left to right
    // y is up to down
    // but for imshow x is up to down, y is left to right
    m_nt_tile_x ++;
    if (m_nt_tile_x >= 32) {
        m_nt_tile_x = 0;
        m_nt_tile_y++;
        if (m_nt_tile_y >= 30) {
            m_nt_tile_y = 0;
        }
    }
    uint8_t y_shift = 0; // (m_ppuscroll_y+1)%8;
    uint8_t x_shift = m_ppuscroll_x % 8;

    uint16_t tile_addr = 0x2000 | (m_ppu_reg_v & 0x0FFF);
    uint16_t attr_addr = 0x23C0 | (m_ppu_reg_v & 0x0C00) | ((m_ppu_reg_v >> 4) & 0x38) | ((m_ppu_reg_v >> 2) & 0x07);
    uint8_t sprite_no = m_vram[tile_addr];
    uint8_t palette_no = 8; // ((m_vram[attr_addr] >> attr_bitshift) & 0b11);

    bool table_no = get_ppuctrl_bit(PPUCTRL_BGPATTTABLE);
    uint8_t actual_x_shift = 0;
    if (m_nt_tile_x >= 1) {
        actual_x_shift = x_shift;
    }
    add_sprite(&m_next_frame, sprite_no, table_no, m_nt_tile_x*8-actual_x_shift, m_nt_tile_y*8-y_shift, palette_no, false, false, false);
}

void PpuDevice::dbg_render_fullnametable(cv::Mat * dbg_frame) {
    static uint16_t prevscroll = 0;
    // x is left to right
    // y is up to down
    // but for imshow x is up to down, y is left to right
    for (int8_t screen_sprite_y = 0; screen_sprite_y < 60; screen_sprite_y++) {
        for (int16_t screen_sprite_x = 0; screen_sprite_x < 64; screen_sprite_x++) {
            uint16_t sprite_x = screen_sprite_x;
            uint16_t sprite_y = screen_sprite_y;
            uint16_t nametable_no = 0;
            if (sprite_y >= 30) {
                sprite_y -= 30;
                nametable_no += 2;
            }
            if (sprite_x >= 32) {
                sprite_x -= 32;
                nametable_no += 1;
            }
            uint16_t nametable_base_addr = 0x2000 + 0x400*nametable_no;
            uint8_t sprite_no = m_vram[nametable_base_addr + sprite_x + sprite_y * 32];
            /*
            7654 3210
            |||| ||++- Color bits 3-2 for top left quadrant of this byte
            |||| ++--- Color bits 3-2 for top right quadrant of this byte
            ||++------ Color bits 3-2 for bottom left quadrant of this byte
            ++-------- Color bits 3-2 for bottom right quadrant of this byte
            */
            uint16_t attribute_table_addr = 0x3c0 +  (static_cast<uint16_t>(sprite_y) / 4)*8 + static_cast<uint16_t>(sprite_x) / 4;
            uint8_t attr_bitshift = 0;
            if (sprite_y % 4 > 1) {
                // bottom
                attr_bitshift += 4;
            }
            if (sprite_x % 4 > 1) {
                // right
                attr_bitshift += 2;
            }
            uint8_t palette_no = ((m_vram[nametable_base_addr + attribute_table_addr] >> attr_bitshift) & 0b11);
            bool table_no = 1; //get_ppuctrl_bit(PPUCTRL_BGPATTTABLE);
            add_sprite(dbg_frame, sprite_no, table_no, screen_sprite_x*8, screen_sprite_y*8, palette_no, false, false, false);
        }
    }

    for (uint16_t x = 0; x < 60*8; x++) {
      // skip the zero values (HUD displaying)
      //if (m_ppuscroll_x != 0) {
        prevscroll = m_ppuscroll_x;
      //}
      dbg_frame->at<cv::Vec3b>(x, prevscroll) = cv::Vec3b(255, 0, 0);
    }
}

// renders the various sprites
// used for OAM render
void PpuDevice::render_oam_line(uint8_t line_no) {
    
    bool spritesize = get_ppuctrl_bit(PPUCTRL_SPRITESIZE);

    if (spritesize) {
        throw std::runtime_error("16x8 tiles not supported yet");
    }
    for (int8_t i = 0; i < 64; i++) { // i = sprite no. thus i = 0 => sprite 0 for collision
        uint8_t sprite_y = m_ppuoam[i*4]; // top to bottom
        uint8_t sprite_no = m_ppuoam[i*4+1];
        uint8_t sprite_attr = m_ppuoam[i*4+2];
        uint8_t sprite_x = m_ppuoam[i*4+3]; // left to right
        if (sprite_y < line_no - 7 || sprite_y > line_no) {
            // sprite not on this line
            continue;
        }
        if (sprite_y == 255) {
            // TODO : I guess this should be handled differently!
            continue;
        }
        // TODO : those declare should not rather be outside the loop ? Maybe it is optimized by the compil.
        bool hflip = ((sprite_attr & PPUOAM_ATT_HFLIP) != 0);
        bool vflip = ((sprite_attr & PPUOAM_ATT_VFLIP) != 0);
        uint8_t palette_no = (sprite_attr & 0b11) + 4; // add 4 to reach OAM palette
        bool table_no = get_ppuctrl_bit(PPUCTRL_OAMPATTTABLE);
        // TODO : this is a lot of checks just for the first sprite...
        bool collision;
        if (i==0 && line_no >= 2) {
            collision = add_sprite_line(sprite_no, table_no, sprite_x, sprite_y, line_no - sprite_y, palette_no, hflip, vflip, true, true);
            if (collision && 
                ((m_ppumask & (PPUMASK_ENABLE_BG|PPUMASK_ENABLE_SPRITE))==(PPUMASK_ENABLE_BG|PPUMASK_ENABLE_SPRITE))) {
                m_ppustatus |= PPUSTATUS_SPRITE0_COLLISION;
            }
        } else {
            add_sprite_line(sprite_no, table_no, sprite_x, sprite_y, line_no - sprite_y, palette_no, hflip, vflip, true, false);
        }
    }
}

// used for BG render
void PpuDevice::add_sprite(cv::Mat * frame, uint8_t sprite_no, bool table_no, uint16_t sprite_x, uint16_t sprite_y, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg) { //(self, sprite_no, sprite_table_no, frame, spritex, spritey, palette_no, palettes, hflip, vflip):
    // palette = palettes[palette_no*4:palette_no*4 + 4]
    uint8_t sprite[8][8];
    get_sprite(sprite, sprite_no, table_no, false);
    bool sprite0_collision = false;
    for (uint8_t y = 0; y < 8; y++) {
        // I disable this check, it maybe intentional to have sprites that warps around
        // if (sprite_y + y >= 240) {
        //     continue;
        // }
        for (uint8_t x = 0; x < 8; x++) {
            // if (sprite_x + x >= 256) {
            //     continue;
            // }
            uint8_t pix_color = 0;
            if (!hflip && !vflip) {
                pix_color = sprite[y][x];
            } else if (!vflip) { // only hflip
                pix_color = sprite[y][7-x];
            } else if (!hflip) { // only vflip
                pix_color = sprite[7-y][x];
            } else { // vflip and hflip
                pix_color = sprite[7-y][7-x];
            }
            uint8_t color_no;
            if (pix_color != 0) {
                // 0x3f00 : palettes location in vram
                // a palette : a set of 4 colors (4 bytes then)
                // palette_no : the index of the palette in the palette list
                // pix_color : the color in the palette
                color_no = m_vram[0x3f00 + static_cast<uint16_t>(palette_no) * 4 + static_cast<uint16_t>(pix_color)];
            } else if (!transparent_bg) {
                color_no = m_vram[0x3f10];
                // std::cout << (int) color_no << std::endl;
            } else {
                continue;
            }
            uint8_t r = NES_COLORS[color_no][0]; // TODO : get pointer
            uint8_t g = NES_COLORS[color_no][1];
            uint8_t b = NES_COLORS[color_no][2];
            // TODO there are fatser ways to populate a frame
            frame->at<cv::Vec3b>(sprite_y + y, sprite_x + x) = cv::Vec3b(r, g, b);
        }
    }
}

bool PpuDevice::add_sprite_line(uint8_t sprite_no, bool table_no, uint8_t sprite_x, uint8_t sprite_y, uint8_t sprite_line, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision) { //(self, sprite_no, sprite_table_no, frame, spritex, spritey, palette_no, palettes, hflip, vflip):
    // palette = palettes[palette_no*4:palette_no*4 + 4]
    uint8_t sprite[8];
    get_sprite_line(sprite, sprite_no, table_no, sprite_line, hflip, vflip);
    bool sprite0_collision = false;
    uint8_t bg_color_no = m_vram[0x3f10];
    uint8_t bg_color_r = NES_COLORS[bg_color_no][0];
    uint8_t bg_color_g = NES_COLORS[bg_color_no][1];
    uint8_t bg_color_b = NES_COLORS[bg_color_no][2];
    for (uint8_t x = 0; x < 8; x++) {
        uint8_t pix_color = sprite[x];
        uint8_t color_no;
        if (pix_color != 0) {
            // 0x3f00 : palettes location in vram
            // a palette : a set of 4 colors (4 bytes then)
            // palette_no : the index of the palette in the palette list
            // pix_color : the color in the palette
            color_no = m_vram[0x3f00 + static_cast<uint16_t>(palette_no) * 4 + static_cast<uint16_t>(pix_color)];
        } else if (!transparent_bg) {
            color_no = m_vram[0x3f10];
        } else {
            continue;
        }
        uint8_t r = NES_COLORS[color_no][0]; // TODO : get pointer
        uint8_t g = NES_COLORS[color_no][1];
        uint8_t b = NES_COLORS[color_no][2];
        cv::Vec3b bg_color = m_next_frame.at<cv::Vec3b>(sprite_y + sprite_line, sprite_x + x);
        // TODO : same here,a lot of check for the sprite 0
        if (check_collision && (bg_color[0] != bg_color_r && bg_color[1] != bg_color_g && bg_color[2] != bg_color_b) || true) {
            // background is set
            // TODO : do better, it relies on the background being black and nothing else being black
            sprite0_collision = true;
        }
        // TODO there are fatser ways to populate a frame
        m_next_frame.at<cv::Vec3b>(sprite_y + sprite_line, sprite_x + x) = cv::Vec3b(r, g, b);
    }
    return sprite0_collision;
}

void PpuDevice::get_sprite(uint8_t sprite[8][8], uint8_t sprite_no, bool table_no, bool doubletile) {
    /*
    sprite is a uint8_t[8][8];
    doubletile : 16x8 tile mode
    */
    if (doubletile) {
        throw std::runtime_error("doubletile not implemented yet");
    }

    uint16_t plane0_addr = (sprite_no + 256*table_no) << 4;
    for (uint8_t j = 0; j < 8; j++) {
        uint8_t plane0 = m_chr_rom[plane0_addr + j];
        uint8_t plane1 = m_chr_rom[plane0_addr + j + 8];
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t color0 = (plane0 >> i) & 1;
            uint8_t color1 = (plane1 >> i) & 1;
            uint8_t color = (color1 << 1) + color0;
            sprite[j][7-i] = color;
        }
    }
}

void PpuDevice::get_sprite_line(uint8_t sprite[8], uint8_t sprite_no, bool table_no, uint8_t sprite_line, bool hflip, bool vflip) {
    /*
    sprite is a uint8_t[8];
    doubletile : 16x8 tile mode
    */
    uint8_t local_sprite_line = sprite_line;
    if (vflip) {
        local_sprite_line = 7 - sprite_line;
    }
    uint16_t plane0_addr = (sprite_no + 256*table_no) << 4;
    uint8_t plane0 = m_chr_rom[plane0_addr + local_sprite_line];
    uint8_t plane1 = m_chr_rom[plane0_addr + local_sprite_line + 8];
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t color0 = (plane0 >> i) & 1;
        uint8_t color1 = (plane1 >> i) & 1;
        uint8_t color = (color1 << 1) + color0;
        if (hflip) {
            sprite[i] = color;
        } else {
            sprite[7-i] = color;
        }
    }
}


cv::Mat * PpuDevice::getFrame() {
    return &m_last_frame;
}

void PpuDevice::saveFrame() {
    m_next_frame.copyTo(m_last_frame);
}
