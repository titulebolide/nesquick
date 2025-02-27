import matplotlib.pyplot as plt
import inesparser
import numpy as np
import threading
import evdev

import random

KEY_PPUCTRL = 0
KEY_PPUMASK = 1
KEY_PPUSTATUS = 2
KEY_OAMADDR = 3
KEY_OAMDATA = 4
KEY_PPUSCROLL = 5
KEY_PPUADDR = 6
KEY_PPUDATA = 7
KEY_OAMDMA = 0x2014
KEY_CTRL1 = 0x2016
KEY_CTRL2 = 0x2017


PPUCTRL_VBLANKNMI     = 0b10000000 # 0 : disable NMI on vblank
PPUCTRL_SPRITESIZE    = 0b00100000 # 0 : 8x8, 1: 8x16
PPUCTRL_BGPATTTABLE   = 0b00010000 # Pattern table no select in 8x8 mode
PPUCTRL_OAMPATTTABLE  = 0b00001000 # Pattern table no select in 8x8 mode
PPUCTRL_VRAMINC       = 0b00000100

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

        self.controller_strobe = 0
        self.controller_read_no = 0

        self.keyboard = KbDevice()
        self.keyboard.start()
        self.prevval = 0

        # self.stop = False

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
        if key < 0x2000:
            key %= 8
        if key == KEY_PPUCTRL:
            # print("PPUCTRL")
            self.ppuctrl = value

        elif key == KEY_PPUMASK:
            pass
            # print("mask", bin(value))

        elif key == KEY_PPUADDR:
            # print("PPUADDR")
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
            # print("PPUDATA")
            self.vram[self.ppuaddr] = value
            self.inc_ppuaddr()

        elif key == KEY_PPUSCROLL:
            if value != 0:
                raise Exception

        elif key == KEY_OAMADDR:
            pass
            # print("OAMADDR")

        elif key == KEY_OAMDATA:
            pass
            # print("OAMDATA")

        elif key == KEY_OAMDMA:
            # todo : emulate cpu cycles for DMA ?
            # print("OAMDMA")
            source_addr = (value << 8)
            self.ppuoam = self.cpu_ram[source_addr:source_addr + 256]
            # print(self.ppuoam[0:4])
            # self.stop = True
                
        elif key == KEY_CTRL1:
            self.controller_strobe = (value & 1) # get lsb
            if self.controller_strobe == 1:
                self.controller_read_no = 0
            # print("CTRL1", bin(value))

        else:
            pass
            # print("Unhandled dev reg", hex(key))

        # if self.stop:
        #     self.print_nametable()
        #     # input()


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
            # print("PPUSTATUS")
            return int(random.random()*255.9999) # self.ppustatus
        
        elif key == KEY_CTRL1:
            # TODO : In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. Usually this is the most significant byte of the address of the controller port—0x40. Certain games (such as Paperboy) rely on this behavior and require that reads from the controller ports return exactly $40 or $41 as appropriate. See: Controller reading: unconnected data lines.
            # print("KEY_CTRL1")
            ret = 0
            if self.controller_read_no == 0:
                self.controller_strobe += 1
            # if self.controller_strobe == 4:
            #     ret = 1
            elif self.controller_strobe > 8:
                ret = 1
            else:
                ret = 0
            # print(ret, self.controller_strobe)
            return ret

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
        # if self.ntick == 100:
        #     self.ppustatus = 0b10000000
            
        if self.ntick % 89342 == 89341:
            if self.get_ppuctrl_bit(PPUCTRL_VBLANKNMI) == 1:
                frame1 = self.render_nametable()
                frame = self.render_oam(frame1)

                test = frame.sum() + sum(self.ppuoam)
                if test != self.prevval:
                    self.prevval = test
                    plt.subplot(1, 3, 1)
                    plt.imshow(self.render_nametable())
                    plt.subplot(1, 3, 2)
                    frame3 = self.render_oam(np.zeros((30*8, 32*8)))
                    plt.imshow(frame3)
                    plt.subplot(1, 3, 3)
                    plt.imshow(frame)
                    plt.show()
                self.cpu_interrupt(maskable = False)
                # print("Calling NMI")

    def render_nametable(self):
        nametable_no = self.ppuctrl & 0b11
        self.base_addr = 0x2000 + 0x400*nametable_no
        frame = np.zeros((30*8, 32*8))
        # x is left to right
        # y is up to down
        # but for imshow x is up to down, y is left to right
        table_no = self.get_ppuctrl_bit(PPUCTRL_BGPATTTABLE)
        for sprite_y in range(30):
            for sprite_x in range(32):
                spriteno = self.vram[self.base_addr + sprite_x + sprite_y * 32]
                tile_x = sprite_y*8
                tile_y = sprite_x*8
                sprite = inesparser.ines_get_sprite(self.chr_rom, spriteno, table_no, False)
                frame[tile_x:tile_x+8, tile_y:tile_y+8] = sprite
        return frame
    
    def render_oam(self, frame):
        if self.get_ppuctrl_bit(PPUCTRL_SPRITESIZE) == 0:
            table_no = self.get_ppuctrl_bit(PPUCTRL_OAMPATTTABLE)
            for i in range(64):
                oam_elt = self.ppuoam[i*4:i*4+4]
                sprite_y = oam_elt[0] # top to bottom
                if sprite_y == 255:
                    continue
                sprite_index = oam_elt[1]
                sprite_attributes = oam_elt[2]
                sprite_x = oam_elt[3] # left to right
                sprite = inesparser.ines_get_sprite(self.chr_rom, sprite_index, table_no, False)
                frame[sprite_y:sprite_y+8, sprite_x:sprite_x+8] = sprite
        else:
            raise Exception("16x8 tiles not supported yet")

        return frame
    

class KbDevice(threading.Thread):
    def __init__(self):
        super().__init__()
        self.asciival = 0

    def run(self):
        dev = evdev.InputDevice('/dev/input/event4')
        for event in dev.read_loop():
            if event.type != evdev.ecodes.EV_KEY:
                continue
            key = evdev.ecodes.KEY[event.code][4:]
            if len(key) != 1:
                continue
            if event.value == 0: #press down
                self.asciival = 0
            elif event.value == 1:
                self.asciival = ord(key.lower())
