import numpy as np
import matplotlib.pyplot as plt
import binascii

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
ACCUMULATOR = 11

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
    def __init__(self, lst):
        self.lst = lst
        self.prgm_start = 0x0800
        self.prgm_file = "test.bin"

        self.prgm_ctr = self.prgm_start
        self.regs = [0,0,0,0]
        self.stack_ptr = 0xff

        self.mem = [0]*(2**16)

        # load program to memory
        with open(self.prgm_file, "rb") as f:
            for index, val in enumerate(f.read()):
                self.mem[self.prgm_start + index] = val

        # operation, nbytes
        self.opcodes = {
            0x00: [lambda oc: None, 2**18], # BRK : a big nbbyte will cause the pc to overflow and the program to stop

            0xea: [lambda oc: None, 1], # NOP
            
            ## ADD
            0x69: [lambda oc: self.adc(IMMEDIATE), 2],
            0x65: [lambda oc: self.adc(ZEROPAGE), 2], 
            0x75: [lambda oc: self.adc(ZEROPAGE_X), 2],
            0x6d: [lambda oc: self.adc(ABSOLUTE), 3],
            0x7d: [lambda oc: self.adc(ABSOLUTE_X), 3],
            0x79: [lambda oc: self.adc(ABSOLUTE_Y), 3],
            0x61: [lambda oc: self.adc(PRE_INDEX_INDIRECT), 2],
            0x71: [lambda oc: self.adc(POST_INDEX_INDIRECT), 2],

            ## SUBSTRACT - SBC
            0xe9: [lambda oc: self.sbc(IMMEDIATE), 2],
            0xe5: [lambda oc: self.sbc(ZEROPAGE), 2],
            0xf5: [lambda oc: self.sbc(ZEROPAGE_X), 2],
            0xed: [lambda oc: self.sbc(ABSOLUTE), 3],
            0xfd: [lambda oc: self.sbc(ABSOLUTE_X), 3],
            0xf9: [lambda oc: self.sbc(ABSOLUTE_Y), 3],
            0xe1: [lambda oc: self.sbc(PRE_INDEX_INDIRECT), 2],
            0xf1: [lambda oc: self.sbc(POST_INDEX_INDIRECT), 2],

            ## AND
            0x29: [lambda oc: self.and_(IMMEDIATE), 2],
            0x25: [lambda oc: self.and_(ZEROPAGE), 2], 
            0x35: [lambda oc: self.and_(ZEROPAGE_X), 2],
            0x2D: [lambda oc: self.and_(ABSOLUTE), 3],
            0x3D: [lambda oc: self.and_(ABSOLUTE_X), 3],
            0x39: [lambda oc: self.and_(ABSOLUTE_Y), 3],
            0x21: [lambda oc: self.and_(PRE_INDEX_INDIRECT), 2],
            0x31: [lambda oc: self.and_(POST_INDEX_INDIRECT), 2],

            ## OR
            0x09: [lambda oc: self.or_(IMMEDIATE), 2],
            0x05: [lambda oc: self.or_(ZEROPAGE), 2], 
            0x15: [lambda oc: self.or_(ZEROPAGE_X), 2],
            0x0d: [lambda oc: self.or_(ABSOLUTE), 3],
            0x1d: [lambda oc: self.or_(ABSOLUTE_X), 3],
            0x19: [lambda oc: self.or_(ABSOLUTE_Y), 3],
            0x01: [lambda oc: self.or_(PRE_INDEX_INDIRECT), 2],
            0x11: [lambda oc: self.or_(POST_INDEX_INDIRECT), 2],

            ## CLEAR STATUS
            0x18: [lambda oc: self.set_status_bit(STATUS_CARRY, False), 1], # CLC
            0xd8: [lambda oc: self.set_status_bit(STATUS_DEC, False), 1], # CLD
            0x58: [lambda oc: self.set_status_bit(STATUS_INTER, False), 1], # CLI
            0xb8: [lambda oc: self.set_status_bit(STATUS_OVFLO, False), 1], # CLV

            ## SET STATUS
            0x38: [lambda oc: self.set_status_bit(STATUS_CARRY, True), 1], # SEC
            0xf8: [lambda oc: self.set_status_bit(STATUS_DEC, True), 1], # SED
            0x78: [lambda oc: self.set_status_bit(STATUS_INTER, True), 1], # SEI

            ## BIT SHIFT
            0x4a: [lambda oc: self.shift_right(ACCUMULATOR), 1], # LSR
            0x46: [lambda oc: self.shift_right(ZEROPAGE), 2],
            0x56: [lambda oc: self.shift_right(ZEROPAGE_X), 2],
            0x4e: [lambda oc: self.shift_right(ABSOLUTE), 3],
            0x5e: [lambda oc: self.shift_right(ABSOLUTE_X), 3],

            ## LOADS
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


            ## STORE
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


            ## TRANSFER
            0xaa: [lambda oc: self.tr(REG_A, REG_X), 1], # TAX
            0xa8: [lambda oc: self.tr(REG_A, REG_Y), 1], # TAY
            0xba: [lambda oc: self.tr(REG_S, REG_X), 1], # TSX
            0x8a: [lambda oc: self.tr(REG_X, REG_A), 1], # TXA
            0x9a: [lambda oc: self.tr(REG_X, REG_S), 1], # TXS
            0x98: [lambda oc: self.tr(REG_Y, REG_A), 1], # TYA


            ## COMPARE
            0xc9: [lambda oc: self.cp(REG_A, IMMEDIATE), 2],
            0xc5: [lambda oc: self.cp(REG_A, ZEROPAGE), 2],
            0xd5: [lambda oc: self.cp(REG_A, ZEROPAGE_X), 2],
            0xcd: [lambda oc: self.cp(REG_A, ABSOLUTE), 3],
            0xdd: [lambda oc: self.cp(REG_A, ABSOLUTE_X), 3],
            0xd9: [lambda oc: self.cp(REG_A, ABSOLUTE_Y), 3],
            0xc1: [lambda oc: self.cp(REG_A, PRE_INDEX_INDIRECT), 2],
            0xd1: [lambda oc: self.cp(REG_A, POST_INDEX_INDIRECT), 2],

            0xe0: [lambda oc: self.cp(REG_X, IMMEDIATE), 2],
            0xe4: [lambda oc: self.cp(REG_X, ZEROPAGE), 2],
            0xec: [lambda oc: self.cp(REG_X, ABSOLUTE), 3],

            0xc0: [lambda oc: self.cp(REG_Y, IMMEDIATE), 2],
            0xc4: [lambda oc: self.cp(REG_Y, ZEROPAGE), 2],
            0xcc: [lambda oc: self.cp(REG_Y, ABSOLUTE), 3],


            ## STACK PUSH/PULL
            0x48: [lambda oc: self.ph(REG_A), 1], # PHA
            0x68: [lambda oc: self.pl(REG_A), 1], # PLA
            0x08: [lambda oc: self.ph(REG_S), 1], # PHP
            0x28: [lambda oc: self.pl(REG_S), 1], # PLP


            ## INCREASE / DECREASE
            0xca: [lambda oc: self.in_de_reg(REG_X, sign_plus=False), 1], #DEX
            0x88: [lambda oc: self.in_de_reg(REG_Y, sign_plus=False), 1], #DEY

            0xe8: [lambda oc: self.in_de_reg(REG_X, sign_plus=True), 1], # INX
            0xc8: [lambda oc: self.in_de_reg(REG_Y, sign_plus=True), 1], # INY

            0xc6: [lambda oc: self.in_de_mem(ZEROPAGE, sign_plus=False), 2], # DEC
            0xd6: [lambda oc: self.in_de_mem(ZEROPAGE_X, sign_plus=False), 2], # DEC
            0xce: [lambda oc: self.in_de_mem(ABSOLUTE, sign_plus=False), 3], # DEC
            0xde: [lambda oc: self.in_de_mem(ABSOLUTE_X, sign_plus=False), 3], # DEC


            ## BRANCH
            0xd0: [lambda oc: self.branch(STATUS_ZERO, True), 2], # BNE
            0xf0: [lambda oc: self.branch(STATUS_ZERO, False), 2], # BEQ (inv of BNE)
            0x90: [lambda oc: self.branch(STATUS_CARRY, True), 2], # BCC Branch Carry Clear
            0xb0: [lambda oc: self.branch(STATUS_CARRY, False), 2], # BCS Branch Carry Set
            0x30: [lambda oc: self.branch(STATUS_NEG, False), 2], # BMI Branch on result is negative (mi = minus)
            0x10: [lambda oc: self.branch(STATUS_NEG, True), 2], # BPL Branch on result is positive (pl = plsu)
            0x50: [lambda oc: self.branch(STATUS_OVFLO, True), 2], # BVC Branch on overflow clear
            0x70: [lambda oc: self.branch(STATUS_OVFLO, False), 2], # BVS Branch on overflow set


            ## JUMP
            # 0 byte shift because we handle the shift in the function call
            0x4c: [lambda oc: self.jmp(ABSOLUTE), 0], # JMP
            0x6c: [lambda oc: self.jmp(INDIRECT), 0], # JMP
            0x20: [self.jsr, 0], # JSR
            0x60: [self.rts, 0], # RTS
        }
        print(len(self.opcodes))
        exit(0)

    def set_status_bit(self, status_bit, on):
        if on:
            self.regs[REG_S] |= status_bit
        else:
            self.regs[REG_S] &= byte_not(status_bit)

    def get_addr_val(self, mode):
        if mode in (ABSOLUTE, ABSOLUTE_X, ABSOLUTE_Y):
            addr = self.mem[self.prgm_ctr+1] + self.mem[self.prgm_ctr+2] * 256
            if mode == ABSOLUTE_X:
                addr += self.regs[REG_X]
                addr %= 65536
            elif mode == ABSOLUTE_Y:
                addr += self.regs[REG_Y]
                addr %= 65536

        elif mode in (ZEROPAGE, ZEROPAGE_X, ZEROPAGE_Y):
            addr = self.mem[self.prgm_ctr+1]
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
            
        elif mode == ACCUMULATOR:
            val = self.regs[REG_A]
            return None, val

        elif mode == IMMEDIATE:
            val = self.mem[self.prgm_ctr+1]
            return None, val

        else:
            # relative addressing is only used for branching
            # implicit addressing is operation dependent
            raise Exception
        
        return addr, self.mem[addr]
    
    def update_zn_flag(self, value):
        self.set_status_bit(STATUS_ZERO, value == 0)
        self.set_status_bit(STATUS_NEG, value & 0b10000000 != 0)

    def clear(self, status_bit):
        self.set_status_bit(status_bit, False)
    
    def jmp(self, addr_mode):
        addr, _ = self.get_addr_val(addr_mode)
        self.prgm_ctr = addr
            
    def stack_push(self, val):
        addr = self.stack_ptr + 0x01 * 255
        self.mem[addr] = val
        self.stack_ptr -= 1

    def stack_pull(self):
        self.stack_ptr += 1
        addr = self.stack_ptr + 0x01 * 255
        return self.mem[addr]

    def jsr(self, opcode):
        self.stack_push(self.prgm_ctr + 2)
        self.jmp(ABSOLUTE)

    def rts(self, opcode):
        addr = self.stack_pull()
        self.prgm_ctr = addr + 1

    def ph(self, reg):
        # stack begins at 0x01ff and ends at 0x0100
        if self.stack_ptr == 0:
            raise Exception("Stack overflow")
        self.stack_push(self.regs[reg])

    def pl(self, reg):
        # stack begins at 0x01ff and ends at 0x0100
        if self.stack_ptr == 0xff:
            raise Exception("Empty stack")
        val = self.stack_pull()
        self.regs[reg] = val
        if reg != REG_S:
            # for REG_S it is already handled!
            self.update_zn_flag(val)

    def ld(self, reg, addr_mode):
        # load accumulator
        _, val = self.get_addr_val(addr_mode)
        self.regs[reg] = val
        self.update_zn_flag(val)

    def st(self, reg, addr_mode):   
        if addr_mode == IMMEDIATE:
            raise Exception

        addr, _ = self.get_addr_val(addr_mode)
        val = self.regs[reg]
        self.mem[addr] = val
        self.update_zn_flag(val)
        
    def tr(self, sreg, dreg):
        val = self.regs[sreg]
        self.regs[dreg] = val
        self.update_zn_flag(val)
        
    def cp(self, reg, addr_mode):
        _, val = self.get_addr_val(addr_mode)
        diff = self.regs[reg] - val
        self.update_zn_flag(diff) #status_zero goes to 0 if equality

    def in_de_reg(self, reg, sign_plus):
        # increase or decrease handler
        if sign_plus:
            self.regs[reg] += 1
        else:
            self.regs[reg] -= 1
        self.regs[reg] %= 256
        self.update_zn_flag(self.regs[reg])

    def in_de_mem(self, addr_mode, sign_plus):
        addr, _ = self.get_addr_val(addr_mode)
        if sign_plus:
            self.mem[addr] += 1
        else:
            self.mem[addr] -= 1
        self.mem[addr] %= 256
        self.update_zn_flag(self.mem[addr])

    def add_val_to_acc_carry(self, val):
        if self.regs[REG_S] & STATUS_CARRY != 0:
            # there is a carry
            val += 1
        self.regs[REG_A] += val
        self.set_status_bit(STATUS_CARRY, self.regs[REG_A] > 255)
        self.regs[REG_A] %= 256
        self.update_zn_flag(self.regs[REG_A])

    def adc(self, addr_mode):
        _, val = self.get_addr_val(addr_mode)
        self.add_val_to_acc_carry(val)

    def sbc(self, addr_mode):
        # use two's complement https://stackoverflow.com/a/41253661
        _, val = self.get_addr_val(addr_mode)
        self.add_val_to_acc_carry(byte_not(val))

    def and_(self, addr_mode):
        _, val = self.get_addr_val(addr_mode)
        self.regs[REG_A] |= val
        self.update_zn_flag(self.regs[REG_A])

    def or_(self, addr_mode):
        _, val = self.get_addr_val(addr_mode)
        self.regs[REG_A] |= val
        self.update_zn_flag(self.regs[REG_A])

    def branch(self, status_bit, branch_if_zero):
        do_branch = False
        if self.regs[REG_S] & status_bit == 0:
            do_branch = True
        if not branch_if_zero:
            do_branch = not do_branch
        if do_branch:
            # branch
            branch_addr = self.mem[self.prgm_ctr + 1]
            if branch_addr & 0b10000000 != 0:
                # negative number
                branch_addr -= 256
            self.prgm_ctr += branch_addr

    def shift_right(self, addr_mode):
        if addr_mode == ACCUMULATOR:
            self.regs[REG_A] >>= 1
            self.update_zn_flag(self.regs[REG_A])
        else:
            addr, _ = self.get_addr_val(addr_mode)
            self.mem[addr] >>= 1
            self.update_zn_flag(self.mem[addr])

    def run(self):
        iter = 0
        while True:
            opcode = self.mem[self.prgm_ctr]
            if opcode == 0x00:
                # TODO : handle BRK in a better way (raise interrupt flag)
                break
            if not opcode in self.opcodes:
                raise Exception(f"Unknown opcode {hex(opcode)}")
            func, nbytes = self.opcodes[opcode]
            func(opcode)
            self.prgm_ctr += nbytes
            self.dbg()
            if iter % 500 == 0:
                self.display_mem()
            iter += 1

    def dbg(self):
        print()
        print(self.lst.get_inst(self.prgm_ctr))
        print("PC :", hex(self.prgm_ctr),
              "\tinst :", hex(self.mem[self.prgm_ctr]),
              "\tA :", hex(self.regs[REG_A]),
              "\tX :", hex(self.regs[REG_X]),
              "\tY :", hex(self.regs[REG_Y]),
              "\tS :", bin(self.regs[REG_S]),
              "\tSP :", hex(self.stack_ptr))

    def display_mem(self):
        # 32 x 32 from 0x0200 to 0x05ff
        mat = np.array(self.mem[0x0200:0x0600]).reshape((32,32))
        plt.imshow(mat)
        plt.show()


def lst_addr_to_val(str_addr):
    data = binascii.unhexlify(str_addr)
    val = 0
    for b in data:
        val = val*256 + b
    return val

class LstManager:
    def __init__(self):
        self.inst_map = {}
        with open("test.lst", "r") as f:
            
            for l in f.readlines():
                if l[0] != '0':
                    continue
                addr = lst_addr_to_val(l[0:6])
                inst = l[11:].rstrip("\n").rstrip().lstrip()
                if len(inst) == 0:
                    continue
                self.inst_map[addr] = inst

    def get_inst(self, addr):
        prgm_addr = addr - 0x0800
        if prgm_addr not in self.inst_map:
            return "NOP"
        return self.inst_map[prgm_addr]

l = LstManager()

e = Emulator(l)
e.run()
e.display_mem()
