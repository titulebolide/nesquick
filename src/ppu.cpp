#include <cstdlib>

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
    if (get_ppuctrl_bit(PPUCTRL_VRAMINC)) {
        m_ppuaddr += 32;
    } else {
        m_ppuaddr += 1;
    }
}

void PpuDevice::set(uint16_t addr, uint8_t value) {
    if (addr < 0x4000) {
        addr = ((addr - 0x2000) % 8) + 0x2000; // mirroring every 8 bits
    }
    uint16_t oamdma_source_addr;
    switch (addr)
    {
    case KEY_PPUCTRL:
        m_ppuctrl = value;
        break;
    
    case KEY_PPUMASK:
        break;
    
    case KEY_PPUADDR:
        // done in two reads : msb, then lsb
        if (m_ppu_reg_w == 1) {
            //we are reading the lsb
            m_ppuaddr = (m_ppuaddr << 8) + static_cast<uint16_t>(value);
            m_ppu_reg_w = 0;
        } else {
            // msb, null the most signifants two bits (14 bit long addr space)
            m_ppuaddr = static_cast<uint16_t>(value & 0b00111111);
            m_ppu_reg_w = 1;
        }
        break;

    case KEY_PPUDATA:
        m_vram[m_ppuaddr] = value;
        inc_ppuaddr();
        break;

    case KEY_PPUSCROLL:
        if (m_ppu_reg_w == 0) {
            // 1st write, we are reading X
            m_ppuscroll_x = value;
            m_ppu_reg_w = 1;
        } else if (m_ppu_reg_w) {
            // 2nd write, we are reading Y
            m_ppuscroll_y = value;
            m_ppu_reg_w = 0;
        }
        break;

    case KEY_OAMADDR:
        break;

    case KEY_OAMDATA:
        break;

    case KEY_OAMDMA:
        // todo : emulate cpu cycles for DMA ?
        oamdma_source_addr = (static_cast<uint16_t>(value) << 8);
        for (uint16_t i = 0; i < 256; i ++) {
            m_ppuoam[i] = m_cpu_ram->get(oamdma_source_addr + i);
        }
        break;

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
        break;
    }
}

uint8_t PpuDevice::get(uint16_t addr) {
    uint8_t retval;
    uint8_t controller_state;
    switch (addr) {
    case KEY_PPUDATA:
        // get buffer value
        retval = m_ppudata_buffer;
        // update buffer AFTER the read
        m_ppudata_buffer = m_vram[m_ppuaddr];
        // increase ppuaddr after access
        inc_ppuaddr();
        break;
    
    case KEY_PPUSTATUS:
        // TODO : implement PPUSTATUS for real
        retval = 0b10000000 | m_ppustatus;
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
        
    default:
        break;
    }
    return retval;
}

void PpuDevice::tick() {
    uint16_t scanline_no = m_ntick / SCANLINE_LENGHT;
    uint16_t column_no = m_ntick % SCANLINE_LENGHT;
    if (column_no == 0) {
        // beginning of a new visible scanline
        if (scanline_no < SCANLINE_VBLANK_START) {
            // we are in a visible scanline
            if (scanline_no%8 == 0) {
                // render background batch of 8 lines
                render_nametable_line(scanline_no/8);
                m_dbg_string.append(std::to_string((int)m_ppuscroll_x));
                m_dbg_string.append(" ");
            }
            render_oam_line(scanline_no);
        }
    } else if (column_no == 1) {
        if (scanline_no == SCANLINE_VBLANK_START) {
            // we are in the first tick of vblank
            // Let's finish rendering the frame
            // render_oam();
            saveFrame();
            std::cout << "DBGSCROLL " << m_dbg_string << std::endl;
            m_dbg_string = "";

            // TODO : should not be byte_not a macro or something so it gets notted at compil and not runtime ?
            // TODO : check we are resetting SPRITE0 collision flag at the right moment
            if (get_ppuctrl_bit(PPUCTRL_VBLANKNMI)) {
                m_cpu->interrupt(false);
            }
        } else if (scanline_no == SCANLINE_FLAG_CLEAR) {
            m_ppustatus &= byte_not(PPUSTATUS_SPRITE0_COLLISION);
        }
    }
    m_ntick += 1;
    if (m_ntick == SCANLINE_NUMBER * SCANLINE_LENGHT) {
        m_ntick = 0;
    }
}

// renders the background
void PpuDevice::render_nametable_line(uint8_t screen_sprite_y) {
    // x is left to right
    // y is up to down
    // but for imshow x is up to down, y is left to right
    uint8_t y_shift = 0; // (m_ppuscroll_y+1)%8;
    uint8_t x_shift = 0; //(m_ppuscroll_x+1)%8;
    for (int16_t screen_sprite_x = 0; screen_sprite_x < 32; screen_sprite_x++) {
        uint16_t sprite_x = screen_sprite_x + static_cast<uint16_t>(m_ppuscroll_x/8);
        uint16_t sprite_y = screen_sprite_y + static_cast<uint16_t>(m_ppuscroll_y/8);
        uint16_t nametable_no = m_ppuctrl & 0b11;
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
        bool collision = add_sprite(sprite_no, table_no, screen_sprite_x*8-x_shift, screen_sprite_y*8-y_shift, palette_no, false, false, false, false);
    }
}

// renders the various sprites
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
            if (collision) {
                m_ppustatus |= PPUSTATUS_SPRITE0_COLLISION;
            }
        } else {
            add_sprite_line(sprite_no, table_no, sprite_x, sprite_y, line_no - sprite_y, palette_no, hflip, vflip, true, false);
        }
    }
}

bool PpuDevice::add_sprite(uint8_t sprite_no, bool table_no, uint8_t sprite_x, uint8_t sprite_y, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision) { //(self, sprite_no, sprite_table_no, frame, spritex, spritey, palette_no, palettes, hflip, vflip):
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
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0; // TODO : set here transparent bg
            if (pix_color != 0) {
                // 0x3f00 : palettes location in vram
                // a palette : a set of 4 colors (4 bytes then)
                // palette_no : the index of the palette in the palette list
                // pix_color : the color in the palette
                uint8_t color_no = m_vram[0x3f00 + static_cast<uint16_t>(palette_no) * 4 + static_cast<uint16_t>(pix_color)];
                r = NES_COLORS[color_no][0]; // TODO : get pointer
                g = NES_COLORS[color_no][1];
                b = NES_COLORS[color_no][2];
                cv::Vec3b bg_color = m_next_frame.at<cv::Vec3b>(sprite_y + y, sprite_x + x);
                // TODO : same here,a lot of check for the sprite 0
                if (check_collision && (bg_color[0] != 0 || bg_color[1] != 0 || bg_color[2] != 0)) {
                    // background is set
                    // TODO : do better, it relies on the background being black and nothing else being black
                    sprite0_collision = true;
                }
            } else if (transparent_bg) {
                continue;
            }
            // TODO there are fatser ways to populate a frame
            m_next_frame.at<cv::Vec3b>(sprite_y + y, sprite_x + x) = cv::Vec3b(r, g, b);
        }
    }
    return sprite0_collision;
}

bool PpuDevice::add_sprite_line(uint8_t sprite_no, bool table_no, uint8_t sprite_x, uint8_t sprite_y, uint8_t sprite_line, uint8_t palette_no, bool hflip, bool vflip, bool transparent_bg, bool check_collision) { //(self, sprite_no, sprite_table_no, frame, spritex, spritey, palette_no, palettes, hflip, vflip):
    // palette = palettes[palette_no*4:palette_no*4 + 4]
    uint8_t sprite[8];
    get_sprite_line(sprite, sprite_no, table_no, sprite_line, hflip, vflip);
    bool sprite0_collision = false;
    for (uint8_t x = 0; x < 8; x++) {
        // if (sprite_x + x >= 256) {
        //     continue;
        // }
        uint8_t pix_color = sprite[x];
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0; // TODO : set here transparent bg
        if (pix_color != 0) {
            // 0x3f00 : palettes location in vram
            // a palette : a set of 4 colors (4 bytes then)
            // palette_no : the index of the palette in the palette list
            // pix_color : the color in the palette
            uint8_t color_no = m_vram[0x3f00 + static_cast<uint16_t>(palette_no) * 4 + static_cast<uint16_t>(pix_color)];
            r = NES_COLORS[color_no][0]; // TODO : get pointer
            g = NES_COLORS[color_no][1];
            b = NES_COLORS[color_no][2];
            cv::Vec3b bg_color = m_next_frame.at<cv::Vec3b>(sprite_y + sprite_line, sprite_x + x);
            // TODO : same here,a lot of check for the sprite 0
            if (check_collision && (bg_color[0] != 0 || bg_color[1] != 0 || bg_color[2] != 0)) {
                // background is set
                // TODO : do better, it relies on the background being black and nothing else being black
                sprite0_collision = true;
            }
        } else if (transparent_bg) {
            continue;
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
