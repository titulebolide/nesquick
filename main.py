import numpy as np
import matplotlib.pyplot as plt

REG_A = 0
REG_X = 1
REG_Y = 2
REG_S = 3 # status register

IMMEDIATE = 0
ZEROPAGE = 1
ZEROPAGE_X = 2
ZEROPAGE_Y = 3
ABSOLUTE = 4
ABSOLUTE_X = 5
ABSOLUTE_Y = 6
INDIRECT = 8
PRE_INDEX_INDIRECT = 9
POST_INDEX_INDIRECT = 10

# status register
STATUS_NEG   = 0b10000000
STATUS_OVFLO = 0b01000000
STATUS_BREAK = 0b00010000
STATUS_DEC   = 0b00001000
STATUS_INTER = 0b00000100
STATUS_ZERO  = 0b00000010
STATUS_CARRY = 0b00000001

def byte_not(val):
    return ~val%256

class Emulator:
    def __init__(self):
        self.prgm_ctr = 0
        self.regs = [0,0,0,0]
        self.stack_ptr = 0xff

        self.mem = [0]*(2**16)

        with open("test/test.bin", "rb") as f:
            self.prgm = f.read()

        # operation, nbytes
        self.opcodes = {
            0x00: [lambda oc : None, 2**18], # BRK : a big nbbyte will cause the pc to overflow and the program to stop
            
            0x69: [self.adc, 2], # immediate
            0x65: [self.adc, 2], # zeropage


            0x90: [lambda oc: self.branch(STATUS_CARRY, True), 2], # BCC Branch Carry Clear

            0xaa: [self.tax, 1],

            0xa9: [lambda oc: self.ld(REG_A, IMMEDIATE), 2],
            0xa5: [lambda oc: self.ld(REG_A, ZEROPAGE), 2],
            0xb5: [lambda oc: self.ld(REG_A, ZEROPAGE_X), 2],
            0xad: [lambda oc: self.ld(REG_A, ABSOLUTE), 3],
            0xbd: [lambda oc: self.ld(REG_A, ABSOLUTE_X), 3],
            0xb9: [lambda oc: self.ld(REG_A, ABSOLUTE_Y), 3],
            0xa1: [lambda oc: self.ld(REG_A, PRE_INDEX_INDIRECT), 2],
            0xb1: [lambda oc: self.ld(REG_A, POST_INDEX_INDIRECT), 2],

            0xa2: [lambda oc: self.ld(REG_X, IMMEDIATE), 2],
            0xa6: [lambda oc: self.ld(REG_X, ZEROPAGE), 2],
            0xb6: [lambda oc: self.ld(REG_X, ZEROPAGE_Y), 2],
            0xae: [lambda oc: self.ld(REG_X, ABSOLUTE), 3],
            0xbe: [lambda oc: self.ld(REG_X, ABSOLUTE_Y), 3],

            0xa0: [lambda oc: self.ld(REG_Y, IMMEDIATE), 2],
            0xa4: [lambda oc: self.ld(REG_Y, ZEROPAGE), 2],
            0xb4: [lambda oc: self.ld(REG_Y, ZEROPAGE_X), 2],
            0xac: [lambda oc: self.ld(REG_Y, ABSOLUTE), 3],
            0xbc: [lambda oc: self.ld(REG_Y, ABSOLUTE_X), 3],

            0x85: [lambda oc: self.st(REG_A, ZEROPAGE), 2],
            0x95: [lambda oc: self.st(REG_A, ZEROPAGE_X), 2],
            0x8d: [lambda oc: self.st(REG_A, ABSOLUTE), 3],
            0x9d: [lambda oc: self.st(REG_A, ABSOLUTE_X), 3],
            0x99: [lambda oc: self.st(REG_A, ABSOLUTE_Y), 3],
            0x81: [lambda oc: self.st(REG_A, PRE_INDEX_INDIRECT), 2],
            0x91: [lambda oc: self.st(REG_A, POST_INDEX_INDIRECT), 2],

            0x86: [lambda oc: self.st(REG_X, ZEROPAGE), 2],
            0x96: [lambda oc: self.st(REG_X, ZEROPAGE_Y), 2],
            0x8e: [lambda oc: self.st(REG_X, ABSOLUTE), 3],

            0x84: [lambda oc: self.st(REG_Y, ZEROPAGE), 2],
            0x94: [lambda oc: self.st(REG_Y, ZEROPAGE_X), 2],
            0x8c: [lambda oc: self.st(REG_Y, ABSOLUTE), 3],

            0xaa: [lambda oc: self.tr(REG_A, REG_X), 1],
            0xa8: [lambda oc: self.tr(REG_A, REG_Y), 1],
            0xba: [lambda oc: self.tr(REG_S, REG_X), 1],
            0x8a: [lambda oc: self.tr(REG_X, REG_A), 1],
            0x9a: [lambda oc: self.tr(REG_X, REG_S), 1],
            0x98: [lambda oc: self.tr(REG_Y, REG_A), 1],

            0xc9: [lambda oc: self.cp(REG_A, IMMEDIATE), 2],
            0xc5: [lambda oc: self.cp(REG_A, ZEROPAGE), 2],
            0xd5: [lambda oc: self.cp(REG_A, ZEROPAGE_X), 2],
            0xcd: [lambda oc: self.cp(REG_A, ABSOLUTE), 3],
            0xdd: [lambda oc: self.cp(REG_A, ABSOLUTE_X), 3],
            0xd9: [lambda oc: self.cp(REG_A, ABSOLUTE_Y), 3],
            0xc1: [lambda oc: self.cp(REG_A, PRE_INDEX_INDIRECT), 2],
            0xd1: [lambda oc: self.cp(REG_A, POST_INDEX_INDIRECT), 2],

            0xe8: [lambda oc: self.cp(REG_X, IMMEDIATE), 2],
            0xe4: [lambda oc: self.cp(REG_X, ZEROPAGE), 2],
            0xec: [lambda oc: self.cp(REG_X, ABSOLUTE), 3],

            0xc0: [lambda oc: self.cp(REG_Y, IMMEDIATE), 2],
            0xc4: [lambda oc: self.cp(REG_Y, ZEROPAGE), 2],
            0xcc: [lambda oc: self.cp(REG_Y, ABSOLUTE), 3],

            0x48: [self.pha, 1],
            0x68: [self.pla, 1],

            0xb0: [lambda oc: self.branch(STATUS_CARRY, False), 2], # BCS Branch Carry Set

            0xca: [lambda oc: self.in_de_(REG_X, sign_plus=False), 1],
            0x88: [lambda oc: self.in_de_(REG_Y, sign_plus=False), 1], #DEY

            0xe8: [lambda oc: self.in_de_(REG_X, sign_plus=True), 1], # INX
            0xc8: [lambda oc: self.in_de_(REG_Y, sign_plus=True), 1], # INY

            0xd0: [lambda oc: self.branch(STATUS_ZERO, True), 2], # only relative

            0xf0: [lambda oc: self.branch(STATUS_ZERO, False), 2], #BEQ (inv of BNE)

            0x4c: [lambda oc: self.jmp(ABSOLUTE), 0],
            0x6c: [lambda oc: self.jmp(INDIRECT), 0],
        }

    def set_status_bit(self, status_bit, on):
        if on:
            self.regs[REG_S] |= status_bit
        else:
            self.regs[REG_S] &= not(status_bit)

    def get_addr_val(self, mode):
        if mode in (ABSOLUTE, ABSOLUTE_X, ABSOLUTE_Y):
            addr = self.prgm[self.prgm_ctr+1] + self.prgm[self.prgm_ctr+2] * 256
            if mode == ABSOLUTE_X:
                addr += self.regs[REG_X]
                addr %= 65536
            elif mode == ABSOLUTE_Y:
                addr += self.regs[REG_Y]
                addr %= 65536

        elif mode in (ZEROPAGE, ZEROPAGE_X, ZEROPAGE_Y):
            addr = self.prgm[self.prgm_ctr+1]
            if mode == ZEROPAGE_X:
                addr += self.regs[REG_X]
                addr %= 256 # zeropage wraps
            elif mode == ZEROPAGE_Y:
                addr += self.regs[REG_Y]
                addr %= 256 # zeropage wraps

        elif mode == INDIRECT:
            # the address given by the user point to a memory
            # where there is the lsb of the address we are looking for
            # the msb is at the next address
            # fist we recover the given implici addr:
            implicit_addr, _ = self.get_addr_val(ABSOLUTE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256

        elif mode == PRE_INDEX_INDIRECT:
            # or indexed indirect
            # implicit addr is a ZEROPAGE_X
            implicit_addr, _ = self.get_addr_val(ZEROPAGE_X)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256

        elif mode == POST_INDEX_INDIRECT:
            # or indirect indexed
            # implicit addr is a ZEROPAGE
            # the address we found there is offset by REG_Y
            implicit_addr, _ = self.get_addr_val(ZEROPAGE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256
            addr += self.regs[REG_Y]
            addr %= 65536
            
        elif mode == IMMEDIATE:
            val = self.prgm[self.prgm_ctr+1]
            return None, val

        else:
            # relative addressing is only used for branching
            # implicit addressing is operation dependent
            raise Exception
        
        return addr, self.mem[addr]
    
    def jmp(self, addr_mode):
        addr, _ = self.get_addr_val(addr_mode)
        self.prgm_ctr = addr
            
    def pha(self, oc):
        # stack begins at 0x01ff and ends at 0x0100
        if self.stack_ptr == 0:
            raise Exception("Stack overflow")
        addr = self.stack_ptr + 0x01 * 255
        self.mem[addr] = self.regs[REG_A]
        self.stack_ptr -= 1

    def pla(self, oc):
        # stack begins at 0x01ff and ends at 0x0100
        if self.stack_ptr == 0xff:
            raise Exception("Empty stack")
        self.stack_ptr += 1
        addr = self.stack_ptr + 0x01 * 255
        self.regs[REG_A] = self.mem[addr]

    def ld(self, reg, addr_mode):
        # load accumulator
        _, value = self.get_addr_val(addr_mode)
        self.regs[reg] = value

    def st(self, reg, addr_mode):   
        if addr_mode == IMMEDIATE:
            raise Exception

        addr, _ = self.get_addr_val(addr_mode)
        self.mem[addr] = self.regs[reg]

    def tr(self, sreg, dreg):
        self.regs[dreg] = self.regs[sreg]
        
    def cp(self, reg, addr_mode):
        _, val = self.get_addr_val(addr_mode)
        self.set_status_bit(STATUS_ZERO, val == self.regs[reg])

    def tax(self, opcode):
        if opcode == 0xaa:
            self.regs[REG_X] = self.regs[REG_A]
        else:
            raise Exception

    def in_de_(self, reg, sign_plus=True):
        # increase or decrease handler
        if sign_plus:
            self.regs[reg] += 1
        else:
            self.regs[reg] -= 1
        self.regs[reg] %= 256

    def adc(self, opcode):
        if opcode == 0x69:
            self.regs[REG_A] += self.prgm[self.prgm_ctr+1]
            self.set_status_bit(STATUS_CARRY, self.regs[REG_A] > 255)
            self.regs[REG_A] %= 256

        elif opcode == 0x65:
            # zeropage
            addr = self.prgm[self.prgm_ctr + 1]
            self.regs[REG_A] += self.mem[addr]
            self.set_status_bit(STATUS_CARRY, self.regs[REG_A] > 255)
            self.regs[REG_A] %= 256

        else:
            raise Exception

    def bne(self, opcode):
        self
        if opcode == 0xd0:
            if self.regs[REG_S] & STATUS_ZERO == 0:
                # branch
                branch_addr = self.prgm[self.prgm_ctr + 1]
                if branch_addr & 0b10000000 != 0:
                    # negative number
                    branch_addr -= 256
                self.prgm_ctr += branch_addr
        else:
            raise Exception
        
    def branch(self, status_bit, branch_if_zero):
        do_branch = False
        if self.regs[REG_S] & status_bit == 0:
            do_branch = True
        if not branch_if_zero:
            do_branch = not do_branch
        if do_branch:
            # branch
            branch_addr = self.prgm[self.prgm_ctr + 1]
            if branch_addr & 0b10000000 != 0:
                # negative number
                branch_addr -= 256
            self.prgm_ctr += branch_addr


    def run(self):
        while self.prgm_ctr < len(self.prgm):
            opcode = self.prgm[self.prgm_ctr]
            if not opcode in self.opcodes:
                raise Exception(f"Unknown opcode {hex(opcode)}")
            func, nbytes = self.opcodes[opcode]
            func(opcode)
            self.prgm_ctr += nbytes
            self.dbg()

    def dbg(self):
        print("PC  :", self.prgm_ctr,
              "\tA :", hex(self.regs[REG_A]),
              "\tX :", hex(self.regs[REG_X]),
              "\tY :", hex(self.regs[REG_Y]),
              "\tS :", bin(self.regs[REG_S]),
              "\tSP :", hex(self.stack_ptr))

    def display_mem(self):
        # 32 x 32 from 0x0200 to 0x05ff
        mat = np.array(self.mem[0x0200:0x0600]).reshape((32,32))
        print(mat.shape)
        plt.imshow(mat)
        plt.show()


e = Emulator()
e.run()
e.display_mem()