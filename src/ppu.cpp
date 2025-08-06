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
    if (SCANLINE_LAST_VISIBLE < scanline_no && scanline_no < SCANLINE_PRE_RENDER || true) { // TODO : why is this true fixing the dbg nametable (hence fixing the write in the vram) ? This makes no sense
        // outside rendering
        // quite normal ppu addr incr
        if (get_ppuctrl_bit(PPUCTRL_VRAMINC)) {
            m_ppuaddr += 32;
            m_reg_v += 32;
        } else {
            m_ppuaddr += 1;
            m_reg_v += 1;
        }
    } else {
        // TODO : check rendering is enabled
        coarse_x_incr();
        y_incr();
    }
}

void PpuDevice::coarse_x_incr() {
    if ((m_reg_v & 0x001F) == 31) {
        // if coarse X == 31
        m_reg_v &= ~0x001F;          // coarse X = 0
        m_reg_v ^= 0x0400;           // switch horizontal nametable
    } else {
        m_reg_v += 1;
    }
}

void PpuDevice::y_incr() {
    if ((m_reg_v & 0x7000) != 0x7000) {
        // if fine Y < 7
        m_reg_v += 0x1000;                      // increment fine Y
    } else {
        m_reg_v &= ~0x7000;                     // fine Y = 0
        uint16_t y = (m_reg_v & 0x03E0) >> 5;        // let y = coarse Y
        if (y == 29) {
            y = 0;                          // coarse Y = 0
            m_reg_v ^= 0x0800;                    // switch vertical nametable
        } else if (y == 31) {
            y = 0;                          // coarse Y = 0, nametable not switched
        } else {
            y += 1;                         // increment coarse Y
        }
        m_reg_v = (m_reg_v & ~0x03E0) | (y << 5);     // put coarse Y back into v
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

        clear_bits(&m_reg_t, BIT10|BIT11);
        m_reg_t |= (value & 0b11) << 10;
        break;
    
    case KEY_PPUMASK:
        m_ppumask = value;
        break;
    
    case KEY_PPUADDR:
        // done in two reads : msb, then lsb
        if (m_reg_w == 0) {
            // t: .CDEFGH ........ <- d: ..CDEFGH
            //        <unused>     <- d: AB......
            // t: Z...... ........ <- 0 (bit Z is cleared)
            // w:                  <- 1

            // msb, null the most signifants two bits (14 bit long addr space)
            m_ppuaddr = value16b & 0b00111111;

            // only bits 8->14 are set by value but bit 15 is also cleared 
            clear_bits(&m_reg_t, 0b1111111100000000);
            m_reg_t |= (value16b & 0b00111111) << 8;

        } else {
            // t: ....... ABCDEFGH <- d: ABCDEFGH
            // v: <...all bits...> <- t: <...all bits...>
            // w:                  <- 0

            //we are reading the lsb
            m_ppuaddr = (m_ppuaddr << 8) + value16b;

            clear_bits(&m_reg_t, 0b11111111);
            m_reg_t |= value16b;

            m_reg_v = m_reg_t;
        }
        // flip w
        m_reg_w = !m_reg_w;
        break;

    case KEY_PPUDATA:
        m_vram[m_ppuaddr] = value;
        inc_ppuaddr();
        break;

    case KEY_PPUSCROLL:
        if (m_reg_w == 0) {
            // 1st write, we are reading X

            // t: ....... ...ABCDE <- d: ABCDE...
            // x:              FGH <- d: .....FGH
            // w:                  <- 1

            clear_bits(&m_reg_t, 0b11111);
            m_reg_t |= value16b >> 3;

            m_reg_x = value & 0b111;

        } else {
            // 2nd write, we are reading Y

            // t: FGH..AB CDE..... <- d: ABCDEFGH
            // w:                  <- 0

            clear_bits(&m_reg_t, 0b0111001111100000);
            // set FGH
            m_reg_t |= (value16b & 0b111) << 12;
            // set ABCDE
            m_reg_t |= (value16b & 0b11111000) << 5;
        }
        // flip w
        m_reg_w = !m_reg_w;
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
        m_reg_w = 0;
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

    // TODO : turn this into a lookup tab)le of pointer to exec function for matching columns
    if (column_no == 0) {
        // beginning of a new visible scanline

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
    } else if (8 <= column_no && column_no <= 240 && column_no%8 == 0) {
        render_nametable_segment(column_no/8+1);
    } else if (column_no == 256) {
        // https://www.nesdev.org/wiki/PPU_scrolling#At_dot_256_of_each_scanline
        if (scanline_no == SCANLINE_PRE_RENDER || scanline_no <= SCANLINE_LAST_VISIBLE) {
            y_incr();
            // probably uneeded:
            // coarse_x_incr();
        }
    } else if (column_no == 257) {
        // hori(t) -> hori(v) :
        // v: ....A.. ...BCDEF <- t: ....A.. ...BCDEF
        uint16_t filter = 0b0000010000011111;
        m_reg_v = (m_reg_v & ~filter) | (m_reg_t & filter);

    }  else if (column_no == 258) {
        if (scanline_no < SCANLINE_VBLANK_START) {
            render_oam_scanline(scanline_no);
        }
    } else if (280 <= column_no && column_no <= 304) {
        // https://www.nesdev.org/wiki/PPU_scrolling#During_dots_280_to_304_of_the_pre-render_scanline_(end_of_vblank)
        if (scanline_no == SCANLINE_PRE_RENDER) {
            // vert(t) -> vert(v) :
            // v: GHIA.BC DEF..... <- t: GHIA.BC DEF.....
            uint16_t filter = 0b0111101111100000;
            m_reg_v = (m_reg_v & ~filter) | (m_reg_t & filter);
        }
    } else if (column_no == 328) {
        render_nametable_segment(0);
    } else if (column_no == 336) {
        render_nametable_segment(1);
    } 
    m_ntick++;
    if (m_ntick == SCANLINE_NUMBER * SCANLINE_LENGHT) {
        m_ntick = 0;
        m_n_frame++;
    }
}

// renders the background
void PpuDevice::render_nametable_segment(uint8_t sprite_x) {
    // x is left to right
    // y is up to down
    // but for imshow x is up to down, y is left to right

    uint16_t scanline_no = m_ntick / SCANLINE_LENGHT;
    // only render for visible scanline
    if (!(scanline_no <= SCANLINE_LAST_VISIBLE || scanline_no == SCANLINE_PRE_RENDER)) {
        return;
    }
    uint16_t column_no = m_ntick % SCANLINE_LENGHT;
    if (scanline_no == SCANLINE_PRE_RENDER && sprite_x > 1) {
        return;
    } else if (scanline_no == SCANLINE_LAST_VISIBLE && sprite_x < 2) {
        return;
    }

    uint16_t fine_y = scanline_no;
    if (fine_y == SCANLINE_PRE_RENDER) {
        fine_y = 0;
    } else if (sprite_x < 2) {
        fine_y += 1;
    }

    uint8_t sprite_y = (fine_y)/8;
    uint8_t sprite_line_no = (fine_y) % 8;
    uint16_t tile_addr = 0x2000 | (m_reg_v & 0x0FFF);

    // if (sprite_x == 10) {
    //     // here lineno (i.e. the number of the line of the current rendered sprite)
    //     // should be equal to addrfiney (i.e. the fine y value in the register v)
    //     // if it is not the case thus reg v is badly updated
    //     // same goes for sprite y and addry when there is no scroll
    //     std::cout << "sl " << (int)scanline_no << " finey " << (int) fine_y << " spritey "  << (int) sprite_y << " lineno " << (int) sprite_line_no << std::endl;
    //     uint16_t coarse_x = (m_reg_v & 0b11111);
    //     uint16_t coarse_y = ((m_reg_v>>5) & 0b11111);
    //     uint8_t addr_fine_y = (m_reg_v >> 12)&0b111;
    //     std::cout << "addrx " << (int) (coarse_x)  << " addry " << (int) (coarse_y) << " addrfiney " << (int) addr_fine_y << std::endl;
    // }

    uint16_t attr_addr = 0x23C0 | (m_reg_v & 0x0C00) | ((m_reg_v >> 4) & 0x38) | ((m_reg_v >> 2) & 0x07);
    uint8_t sprite_no = m_vram[tile_addr];
    uint8_t attr_bitshift = 0;
    if (sprite_y % 4 > 1) {
        // bottom
        attr_bitshift += 4;
    }
    if (sprite_x % 4 > 1) {
        // right
        attr_bitshift += 2;
    }
    uint8_t palette_no = ((m_vram[attr_addr] >> attr_bitshift) & 0b11);

    bool table_no = get_ppuctrl_bit(PPUCTRL_BGPATTTABLE);
    // shift by fine x (thus register x)
    add_sprite_line_to_frame(&m_next_frame, sprite_no, table_no, sprite_x*8-m_reg_x, sprite_y*8, sprite_line_no, palette_no, false, false, false, false);
    coarse_x_incr();
}

void PpuDevice::dbg_render_fullnametable(cv::Mat * dbg_frame) {
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
            for (uint8_t line_no = 0; line_no < 8; line_no++) {
                add_sprite_line_to_frame(dbg_frame, sprite_no, table_no, screen_sprite_x*8, screen_sprite_y*8, line_no, palette_no, false, false, false, false, 512, 480);
            }
        }
    }
}

// renders the various sprites
// used for OAM render
void PpuDevice::render_oam_scanline(uint8_t line_no) {
    
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
            collision = add_sprite_line_to_frame(&m_next_frame, sprite_no, table_no, sprite_x, sprite_y, line_no - sprite_y, palette_no, hflip, vflip, true, true);
            if (collision && 
                ((m_ppumask & (PPUMASK_ENABLE_BG|PPUMASK_ENABLE_SPRITE))==(PPUMASK_ENABLE_BG|PPUMASK_ENABLE_SPRITE))) {
                m_ppustatus |= PPUSTATUS_SPRITE0_COLLISION;
            }
        } else {
            add_sprite_line_to_frame(&m_next_frame, sprite_no, table_no, sprite_x, sprite_y, line_no - sprite_y, palette_no, hflip, vflip, true, false);
        }
    }
}

bool PpuDevice::add_sprite_line_to_frame(cv::Mat * frame, uint8_t sprite_no, bool table_no, uint16_t sprite_x, uint16_t sprite_y, uint8_t sprite_line, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision, uint16_t frame_width, uint16_t frame_height) { 
    uint8_t sprite[8];
    get_sprite_line_from_rom(sprite, sprite_no, table_no, sprite_line, hflip, vflip);
    bool sprite0_collision = false;
    uint8_t bg_color_no = m_vram[0x3f10];
    uint8_t bg_color_r = NES_COLORS[bg_color_no][0];
    uint8_t bg_color_g = NES_COLORS[bg_color_no][1];
    uint8_t bg_color_b = NES_COLORS[bg_color_no][2];
    uint16_t frame_y = (sprite_y + sprite_line) % frame_height;
    for (uint8_t x = 0; x < 8; x++) {
        uint16_t frame_x = (sprite_x + x) % frame_width;
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
        cv::Vec3b bg_color = frame->at<cv::Vec3b>(frame_y, frame_x);
        // TODO : same here,a lot of check for the sprite 0
        if (check_collision && (bg_color[0] != bg_color_r && bg_color[1] != bg_color_g && bg_color[2] != bg_color_b)) {
            // background is set
            // TODO : do better, it relies on the background being black and nothing else being black
            sprite0_collision = true;
        }
        // TODO there are fatser ways to populate a frame
        frame->at<cv::Vec3b>(frame_y, frame_x) = cv::Vec3b(r, g, b);
    }
    return sprite0_collision;
}

void PpuDevice::get_sprite_line_from_rom(uint8_t sprite[8], uint8_t sprite_no, bool table_no, uint8_t sprite_line, bool hflip, bool vflip) {
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
