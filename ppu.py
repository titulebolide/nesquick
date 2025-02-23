KEY_PPUCTRL = 0
KEY_PPUSTATUS = 2
KEY_OAMADDR = 3
KEY_OAMDATA = 4
KEY_PPUSCROLL = 5
KEY_PPUADDR = 6
KEY_PPUDATA = 7
KEY_OAMDMA = 0x2014

PPUCTRL_VRAMINC = 0b00000010

# TODO : set OAMADDR to 0 during sprite tile loading

class PpuApuIODevice:
    def __init__(self):
        self.cpu_interrupt = None

        self.vram = [0]*0x4000 # 14 bit addr space
        self.ntick = 0
        self.ppu_reg_w = 0
        self.ppuaddr = 0
        self.ppuctrl = 0
        self.ppustatus = 0
        self.ppudata_buffer = 0 # ppudata does not read directly ram but a buffer that is updated after each read

    def set_cpu_interrupt(self, cpu_interrupt):
        self.cpu_interrupt = cpu_interrupt

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
            print(hex(self.ppuaddr))
            

        elif key == KEY_PPUDATA:
            print("PPUDATA")
            self.vram[self.ppuaddr] = value
            self.inc_ppuaddr()
            print(hex(self.ppuaddr))

        elif key == KEY_OAMADDR:
            print("OAMADDR")

        elif key == KEY_OAMDATA:
            print("OAMDATA")

        elif key == KEY_OAMDMA:
            print("OAMDMA")
            input()

        else:
            print("Unhandled dev reg", key)


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
            
        if self.ntick == 200000:
            self.cpu_interrupt(maskable = False)
            print("Calling NMI")
        # print(self.ntick)
        # print(self.mem[1:10])
