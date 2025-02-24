import matplotlib.pyplot as plt
import inesparser
import numpy as np

KEY_PPUCTRL = 0
KEY_PPUSTATUS = 2
KEY_OAMADDR = 3
KEY_OAMDATA = 4
KEY_PPUSCROLL = 5
KEY_PPUADDR = 6
KEY_PPUDATA = 7
KEY_OAMDMA = 0x2014

PPUCTRL_SPRITESIZE = 0b00100000 # 0 : 8x8, 1: 8x16
PPUCTRL_PATTTABLE  = 0b00001000 # Pattern table no select in 8x8 mode
PPUCTRL_VRAMINC    = 0b00000100

# TODO : set OAMADDR to 0 during sprite tile loading

class PpuApuIODevice:
    def __init__(self, chr_rom):
        self.cpu_interrupt = None

        self.chr_rom = chr_rom
        self.vram = [0]*0x4000 # 14 bit addr space
        self.ntick = 0
        self.ppu_reg_w = 0
        self.ppuaddr = 0
        self.ppuctrl = 0
        self.ppustatus = 0
        self.ppuoam = [0]*256
        self.ppudata_buffer = 0 # ppudata does not read directly ram but a buffer that is updated after each read

        self.stop = False

    def set_cpu_interrupt(self, cpu_interrupt):
        self.cpu_interrupt = cpu_interrupt

    def set_cpu_ram(self, cpu_ram):
        """
        Used for PPU OAMDMA
        Not really proud of doing it this way
        """
        self.cpu_ram = cpu_ram

    def get_ppuctrl_bit(self, status_bit):
        if self.ppuctrl & status_bit != 0:
            return 1
        return 0
    
    def inc_ppuaddr(self):
        if self.get_ppuctrl_bit(PPUCTRL_VRAMINC) == 1:
            self.ppuaddr += 32
        else:
            self.ppuaddr += 1

    def __setitem__(self, key, value):
        if key == KEY_PPUCTRL:
            print("PPUCTRL")
            self.ppuctrl = value

        elif key == KEY_OAMADDR:
            print("OAMADDR")

        elif key == KEY_PPUADDR:
            print("PPUADDR")
            # done in two reads : msb, then lsb
            if self.ppu_reg_w == 1:
                # we are reading the lsb
                self.ppuaddr = (self.ppuaddr << 8) + value
                self.ppu_reg_w = 0
            else:
                # msb, null the most signifants two bits (14 bit long addr space)
                self.ppuaddr = value & 0b00111111
                self.ppu_reg_w = 1
            

        elif key == KEY_PPUDATA:
            print("PPUDATA")
            self.vram[self.ppuaddr] = value
            self.inc_ppuaddr()

        elif key == KEY_OAMADDR:
            print("OAMADDR")

        elif key == KEY_OAMDATA:
            print("OAMDATA")

        elif key == KEY_OAMDMA:
            # todo : emulate cpu cycles for DMA ?
            print("OAMDMA")
            source_addr = (value << 8)
            self.ppuoam = self.cpu_ram[source_addr:source_addr + 256]
            self.stop = True

        else:
            print("Unhandled dev reg", key)

        if self.stop:
            self.print_nametable()
            # input()


    def __getitem__(self, key):
        if key == KEY_PPUDATA:
            # get buffer value
            ret = self.ppudata_buffer
            # update buffer AFTER the read
            self.ppudata_buffer = self.vram[self.ppuaddr]
            # increase ppuaddr after access
            self.inc_ppuaddr()
            return ret

        elif key == KEY_PPUSTATUS:
            print("PPUSTATUS")
            return self.ppustatus

        else:
            ret = 0 # TODO
        # if key == 2:
        #     # PPU STATUS
        #     pass
        #     # self.mem[key] = ret & 0b01111111 # clear vblank byte on read
        # elif key == 4:
        #     # OAMDATA
        
        # else:
        #     pass
        return ret
    
    def tick(self):
        self.ntick += 1
        if self.ntick == 100:
            self.ppustatus = 0b10000000
            
        if self.ntick % 20000 == 19999:
            self.cpu_interrupt(maskable = False)
            print("Calling NMI")

    def print_nametable(self):
        tile = np.zeros((30*8, 32*8))
        # x is left to right
        # y is up to down
        # but for imshow x is up to down, y is left to right
        if self.get_ppuctrl_bit(PPUCTRL_SPRITESIZE) == 0:
            # 8x8 tiles
            table_no = self.get_ppuctrl_bit(PPUCTRL_PATTTABLE)
            for sprite_y in range(30):
                for sprite_x in range(32):
                    spriteno = self.vram[0x2000 + sprite_x + sprite_y * 32]
                    tile_x = sprite_y*8
                    tile_y = sprite_x*8
                    sprite = inesparser.ines_get_sprite(self.chr_rom, spriteno + 256, table_no, False)
                    tile[tile_x:tile_x+8, tile_y:tile_y+8] = sprite

        else:
            raise Exception("16x8 tiles not supported yet")

        plt.imshow(tile)
        plt.show()
