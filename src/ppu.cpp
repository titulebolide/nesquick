#include <cstdlib>

#include "ppu.hpp"

PpuDevice::PpuDevice(uint8_t * _chr_rom, Device * cpu_ram) : 
    cpu_ram(cpu_ram), cpu(nullptr) {

    for (uint16_t addr = 0; addr < 0x4000; addr ++) {
        chr_rom[addr] = _chr_rom[addr];
    }
}


    // def set_cpu_interrupt(self, cpu_interrupt):
    //     cpu_interrupt = cpu_interrupt

    // def set_cpu_ram(self, cpu_ram):
    //     """
    //     Used for PPU OAMDMA
    //     Not really proud of doing it this way
    //     """
    //     cpu_ram = cpu_ram

void PpuDevice::set_cpu(Emu6502 *_cpu) {
    cpu = _cpu;
}

bool PpuDevice::get_ppuctrl_bit(uint8_t status_bit) {
    return (ppuctrl & status_bit != 0);
}

void PpuDevice::inc_ppuaddr() {
    if (get_ppuctrl_bit(PPUCTRL_VRAMINC)) {
        ppuaddr += 32;
    } else {
        ppuaddr += 1;
    }
}

void PpuDevice::set(uint16_t addr, uint8_t value) {

    // if (key < 0x2000) {
    //     key %= 8
    // }
    uint16_t oamdma_source_addr;
    switch (addr)
    {
    case KEY_PPUCTRL:
        ppuctrl = value;
        break;
    
    case KEY_PPUMASK:
        break;
    
    case KEY_PPUADDR:
        // done in two reads : msb, then lsb
        if (ppu_reg_w == 1) {
            //we are reading the lsb
            ppuaddr = (ppuaddr << 8) + static_cast<uint16_t>(value);
            ppu_reg_w = 0;
        } else {
            // msb, null the most signifants two bits (14 bit long addr space)
            ppuaddr = static_cast<uint16_t>(value & 0b00111111);
            ppu_reg_w = 1;
        }
        break;

    case KEY_PPUDATA:
        vram[ppuaddr] = value;
        inc_ppuaddr();
        break;

    case KEY_PPUSCROLL:
        if (value != 0) {
            throw std::runtime_error("scroll not implemented");
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
            ppuoam[i] = cpu_ram->get(oamdma_source_addr + i);
        }
        break;

    // case KEY_CTRL1:
    //     controller_strobe = (value & 1) # get lsb
    //     if controller_strobe == 1:
    //         controller_read_no = 0
    //     break:

    
    default:
        break;
    }
    

}

uint8_t PpuDevice::get(uint16_t addr) {
    uint8_t retval;
    switch (addr)
    {
    case KEY_PPUDATA:
        // get buffer value
        retval = ppudata_buffer;
        // update buffer AFTER the read
        ppudata_buffer = vram[ppuaddr];
        // increase ppuaddr after access
        inc_ppuaddr();
        break;
    
    case KEY_PPUSTATUS:
        // TODO : do better than that!!
        retval = std::rand() % 256; // ppustatus
        break;
    
    // case KEY_CTRL1:
    //     // TODO : In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. Usually this is the most significant byte of the address of the controller port—0x40. Certain games (such as Paperboy) rely on this behavior and require that reads from the controller ports return exactly $40 or $41 as appropriate. See: Controller reading: unconnected data lines.
    //     controller_state = keyboard_state.value

    //     if controller_read_no > 7:
    //         retval = 1
    //     else:
    //         retval = ((controller_state >> controller_read_no) & 1)

    //     if controller_strobe == 0:
                // TODOD dont increase if over 7 ! useless
    //         controller_read_no += 1

        
    default:
        break;
    }
    return retval;
}

void PpuDevice::tick() {
    ntick += 1;
    if (ntick % 89342 == 89341){
        ntick = 0;
        if (get_ppuctrl_bit(PPUCTRL_VBLANKNMI)) {
            cpu->interrupt(false);
        }
    }
            // nametable_no = ppuctrl & 0b11
            // nametable_base_addr = 0x2000 + 0x400*nametable_no
            // renderer_queue.put_nowait(
            //     {
            //         "bg_table_no":get_ppuctrl_bit(PPUCTRL_BGPATTTABLE),
            //         "nametable":vram[nametable_base_addr:nametable_base_addr+0x400],
            //         "sprite_table_no":get_ppuctrl_bit(PPUCTRL_OAMPATTTABLE),
            //         "ppuoam":ppuoam,
            //         "spritesize":get_ppuctrl_bit(PPUCTRL_SPRITESIZE),
            //         "palettes":vram[0x3f00:0x3f20]
            //     }
            // )
}