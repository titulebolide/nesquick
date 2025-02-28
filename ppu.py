import inesparser
import numpy as np
import threading
import evdev
import cv2 as cv
import multiprocessing
import random
import time

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

PPUOAM_ATT_HFLIP = 0b01000000
PPUOAM_ATT_VFLIP = 0b10000000

CONTROLLER_MAPPING = [112, 111, 98, 110, 119, 115, 97, 100] # A, B, Select, Start, Up, Down, Left, Right

# TODO : set OAMADDR to 0 during sprite tile loading

class PpuApuIODevice:
    def __init__(self, chr_rom, renderer_queue, kbval):
        self.chr_rom = chr_rom
        self.keyboard_val = kbval
        self.renderer_queue = renderer_queue
        
        self.cpu_interrupt = None
        self.cpu_ram = None
        
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
            self.ppuctrl = value

        elif key == KEY_PPUMASK:
            pass

        elif key == KEY_PPUADDR:
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
            self.vram[self.ppuaddr] = value
            self.inc_ppuaddr()

        elif key == KEY_PPUSCROLL:
            if value != 0:
                raise Exception

        elif key == KEY_OAMADDR:
            pass

        elif key == KEY_OAMDATA:
            pass

        elif key == KEY_OAMDMA:
            # todo : emulate cpu cycles for DMA ?
            source_addr = (value << 8)
            self.ppuoam = self.cpu_ram[source_addr:source_addr + 256]
                
        elif key == KEY_CTRL1:
            self.controller_strobe = (value & 1) # get lsb
            if self.controller_strobe == 1:
                self.controller_read_no = 0

        else:
            pass


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
            # TODO : do better !!
            return int(random.random()*255.9999) # self.ppustatus
        
        elif key == KEY_CTRL1:
            # TODO : In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. Usually this is the most significant byte of the address of the controller port—0x40. Certain games (such as Paperboy) rely on this behavior and require that reads from the controller ports return exactly $40 or $41 as appropriate. See: Controller reading: unconnected data lines.
            ret = 0
            pressed_key = self.keyboard_val.value
            
            if self.controller_read_no > 7:
                ret = 1
            elif pressed_key in CONTROLLER_MAPPING:
                if self.controller_read_no == CONTROLLER_MAPPING.index(pressed_key):
                    ret = 1

            if self.controller_strobe == 0:
                self.controller_read_no += 1
            return ret

        else:
            ret = 0 # TODO

        return ret
    
    def tick(self):
        self.ntick += 1
        if self.ntick % 89342 == 89341:
            if self.get_ppuctrl_bit(PPUCTRL_VBLANKNMI) == 1:
                self.cpu_interrupt(maskable = False)
                nametable_no = self.ppuctrl & 0b11
                nametable_base_addr = 0x2000 + 0x400*nametable_no
                self.renderer_queue.put_nowait(
                    {
                        "bg_table_no":self.get_ppuctrl_bit(PPUCTRL_BGPATTTABLE),
                        "nametable":self.vram[nametable_base_addr:nametable_base_addr+960],
                        "sprite_table_no":self.get_ppuctrl_bit(PPUCTRL_OAMPATTTABLE),
                        "ppuoam":self.ppuoam,
                        "spritesize":self.get_ppuctrl_bit(PPUCTRL_SPRITESIZE),
                    }
                )

class PpuRenderer():
    def __init__(self, chr_rom, queue):
        self.queue = queue
        self.chr_rom = chr_rom

    def run(self):
        while True:
            length = self.queue.qsize()
            if length == 0:
                time.sleep(0.001)
                continue
            ppu_state = None
            for i in range(length):
               ppu_state = self.queue.get()
            frame = np.zeros((30*8, 32*8))
            frame = self.render_nametable(
                frame,
                ppu_state["bg_table_no"],
                ppu_state["nametable"],
            )
            self.render_oam(
                frame,
                ppu_state["sprite_table_no"],
                ppu_state["ppuoam"],
                ppu_state["spritesize"],
            )
            cv.imshow("frame", frame)
            cv.waitKey(33)

    def render_nametable(self, frame, bg_table_no, nametable):
        # x is left to right
        # y is up to down
        # but for imshow x is up to down, y is left to right
        for sprite_y in range(30):
            for sprite_x in range(32):
                spriteno = nametable[sprite_x + sprite_y * 32]
                tile_x = sprite_y*8
                tile_y = sprite_x*8
                sprite = inesparser.ines_get_sprite(self.chr_rom, spriteno, bg_table_no, False)
                frame[tile_x:tile_x+8, tile_y:tile_y+8] = sprite
        return frame
    
    def render_oam(self, frame, sprite_table_no, ppuoam, spritesize):
        if spritesize == 0:
            for i in range(64):
                oam_elt = ppuoam[i*4:i*4+4]
                sprite_y = oam_elt[0] # top to bottom
                if sprite_y == 255:
                    continue
                sprite_index = oam_elt[1]
                sprite_attributes = oam_elt[2]
                hflip = ((sprite_attributes & PPUOAM_ATT_HFLIP) != 0)
                vflip = ((sprite_attributes & PPUOAM_ATT_VFLIP) != 0)
                sprite_x = oam_elt[3] # left to right
                sprite = inesparser.ines_get_sprite(self.chr_rom, sprite_index, sprite_table_no, False)
                for x in range(8):
                    for y in range(8):
                        pix_color = 0
                        if not hflip and not vflip:
                            pix_color = sprite[y,x]
                        elif not vflip: # only hflip
                            pix_color = sprite[y,7-x]
                        elif not hflip: # only vflip
                            pix_color = sprite[7-y,x]
                        else: # vflip and hflip
                            pix_color = sprite[7-y,7-x]
                        if pix_color == 0:
                            continue
                        try:
                            frame[sprite_y + y, sprite_x + x] = pix_color
                        except IndexError:
                            continue

        else:
            raise Exception("16x8 tiles not supported yet")
    

class KbDevice(threading.Thread):
    def __init__(self):
        super().__init__()
        self.asciival = multiprocessing.Value("i", 0)

    def run(self):
        dev = evdev.InputDevice('/dev/input/event3')
        for event in dev.read_loop():
            if event.type != evdev.ecodes.EV_KEY:
                continue
            key = evdev.ecodes.KEY[event.code][4:]
            if len(key) != 1:
                continue
            if event.value == 0: #press down
                self.asciival.value = 0
            elif event.value == 1:
                self.asciival.value = ord(key.lower())
