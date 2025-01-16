import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import binascii
import threading
import random
import time

plt.rcParams['keymap.save'].remove('s')
plt.rcParams['keymap.quit'].remove('q')

REG_A = 0
REG_X = 1
REG_Y = 2
REG_S = 3 # status register

IMPLICIT = -1
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

NOEC = 0
YESEC = 1
BRANCHEC = 2

def byte_not(val):
    return ~val%256

def dec2hex(val):
    val = hex(val)[2:]
    if len(val) == 1:
        val = "0" + val
    return val

class Emulator(threading.Thread):
    def __init__(self, lst):
        super().__init__()
        self.lst = lst
        self.prgm_start = 0x0800
        self.prgm_file = "test.bin"

        self.prgm_ctr = self.prgm_start
        self.regs = [0,0,0,0]
        self.stack_ptr = 0xff

        self.mem = [0]*(2**16)

        self.instuction_cycle = 0
        self.instruction_nbcycles = 0

        # load program to memory
        with open(self.prgm_file, "rb") as f:
            for index, val in enumerate(f.read()):
                self.mem[self.prgm_start + index] = val

        # operation, nbytes, ncycles, extracycles
        self.opcodes = {
            0x00: [lambda : None, IMPLICIT, 2**18, 7, NOEC], # BRK : a big nbbyte will cause the pc to overflow and the program to stop

            0xea: [lambda : None, IMPLICIT, 1, 2, NOEC], # NOP


            # BIT TEST
            0x24: [self.bit, ZEROPAGE, 2, 3, NOEC],
            0x2c: [self.bit, ABSOLUTE, 3, 4, NOEC],
            

            ## ADD
            0x69: [self.adc, IMMEDIATE, 2, 2, NOEC],
            0x65: [self.adc, ZEROPAGE, 2, 3, NOEC], 
            0x75: [self.adc, ZEROPAGE_X, 2, 4, NOEC],
            0x6d: [self.adc, ABSOLUTE, 3, 4, NOEC],
            0x7d: [self.adc, ABSOLUTE_X, 3, 4, YESEC],
            0x79: [self.adc, ABSOLUTE_Y, 3, 4, YESEC],
            0x61: [self.adc, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0x71: [self.adc, POST_INDEX_INDIRECT, 2, 5, YESEC],


            ## SUBSTRACT - SBC
            0xe9: [self.sbc, IMMEDIATE, 2, 2, NOEC],
            0xe5: [self.sbc, ZEROPAGE, 2, 3, NOEC],
            0xf5: [self.sbc, ZEROPAGE_X, 2, 4, NOEC],
            0xed: [self.sbc, ABSOLUTE, 3, 4, NOEC],
            0xfd: [self.sbc, ABSOLUTE_X, 3, 4, YESEC],
            0xf9: [self.sbc, ABSOLUTE_Y, 3, 4, YESEC],
            0xe1: [self.sbc, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0xf1: [self.sbc, POST_INDEX_INDIRECT, 2, 5, YESEC],


            ## AND
            0x29: [self.and_, IMMEDIATE, 2, 2, NOEC],
            0x25: [self.and_, ZEROPAGE, 2, 3, NOEC], 
            0x35: [self.and_, ZEROPAGE_X, 2, 4, NOEC],
            0x2D: [self.and_, ABSOLUTE, 3, 4, NOEC],
            0x3D: [self.and_, ABSOLUTE_X, 3, 4, YESEC],
            0x39: [self.and_, ABSOLUTE_Y, 3, 4, YESEC],
            0x21: [self.and_, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0x31: [self.and_, POST_INDEX_INDIRECT, 2, 5, YESEC],


            ## ORA
            0x09: [self.or_, IMMEDIATE, 2, 2, NOEC],
            0x05: [self.or_, ZEROPAGE, 2, 3, NOEC], 
            0x15: [self.or_, ZEROPAGE_X, 2, 4, NOEC],
            0x0d: [self.or_, ABSOLUTE, 3, 4, NOEC],
            0x1d: [self.or_, ABSOLUTE_X, 3, 4, YESEC],
            0x19: [self.or_, ABSOLUTE_Y, 3, 4, YESEC],
            0x01: [self.or_, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0x11: [self.or_, POST_INDEX_INDIRECT, 2, 5, YESEC],


            ## CLEAR STATUS
            0x18: [lambda : self.set_status_bit(STATUS_CARRY, False), IMPLICIT, 1, 2, NOEC], # CLC
            0xd8: [lambda : self.set_status_bit(STATUS_DEC, False), IMPLICIT, 1, 2, NOEC], # CLD
            0x58: [lambda : self.set_status_bit(STATUS_INTER, False), IMPLICIT, 1, 2, NOEC], # CLI
            0xb8: [lambda : self.set_status_bit(STATUS_OVFLO, False), IMPLICIT, 1, 2, NOEC], # CLV


            ## SET STATUS
            0x38: [lambda : self.set_status_bit(STATUS_CARRY, True), IMPLICIT, 1, 2, NOEC], # SEC
            0xf8: [lambda : self.set_status_bit(STATUS_DEC, True), IMPLICIT, 1, 2, NOEC], # SED
            0x78: [lambda : self.set_status_bit(STATUS_INTER, True), IMPLICIT, 1, 2, NOEC], # SEI


            ## BIT SHIFT
            # LSR
            0x4a: [self.shift_right_accumulator, ACCUMULATOR, 1, 2, NOEC],
            0x46: [self.shift_right_memory, ZEROPAGE, 2, 5, NOEC],
            0x56: [self.shift_right_memory, ZEROPAGE_X, 2, 6, NOEC],
            0x4e: [self.shift_right_memory, ABSOLUTE, 3, 6, NOEC],
            0x5e: [self.shift_right_memory, ABSOLUTE_X, 3, 7, NOEC],


            ## LOADS
            # LDA
            0xa9: [self.lda, IMMEDIATE, 2, 2, NOEC], #
            0xa5: [self.lda, ZEROPAGE, 2, 3, NOEC],
            0xb5: [self.lda, ZEROPAGE_X, 2, 4, NOEC],
            0xad: [self.lda, ABSOLUTE, 3, 4, NOEC],
            0xbd: [self.lda, ABSOLUTE_X, 3, 4, YESEC],
            0xb9: [self.lda, ABSOLUTE_Y, 3, 4, YESEC],
            0xa1: [self.lda, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0xb1: [self.lda, POST_INDEX_INDIRECT, 2, 5, YESEC],

            # LDX
            0xa2: [self.ldx, IMMEDIATE, 2, 2, NOEC], # LDX
            0xa6: [self.ldx, ZEROPAGE, 2, 3, NOEC],
            0xb6: [self.ldx, ZEROPAGE_Y, 2, 4, NOEC],
            0xae: [self.ldx, ABSOLUTE, 3, 4, NOEC],
            0xbe: [self.ldx, ABSOLUTE_Y, 3, 4, YESEC],

            # LDY
            0xa0: [self.ldy, IMMEDIATE, 2, 2, NOEC],
            0xa4: [self.ldy, ZEROPAGE, 2, 3, NOEC],
            0xb4: [self.ldy, ZEROPAGE_X, 2, 4, NOEC],
            0xac: [self.ldy, ABSOLUTE, 3, 4, NOEC],
            0xbc: [self.ldy, ABSOLUTE_X, 3, 4, YESEC],


            ## STORE
            0x85: [self.sta, ZEROPAGE, 2, 3, NOEC],
            0x95: [self.sta, ZEROPAGE_X, 2, 4, NOEC],
            0x8d: [self.sta, ABSOLUTE, 3, 4, NOEC],
            0x9d: [self.sta, ABSOLUTE_X, 3, 5, NOEC],
            0x99: [self.sta, ABSOLUTE_Y, 3, 5, NOEC],
            0x81: [self.sta, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0x91: [self.sta, POST_INDEX_INDIRECT, 2, 6, NOEC],

            0x86: [self.stx, ZEROPAGE, 2, 3, NOEC],
            0x96: [self.stx, ZEROPAGE_Y, 2, 4, NOEC],
            0x8e: [self.stx, ABSOLUTE, 3, 4, NOEC],

            0x84: [self.sty, ZEROPAGE, 2, 3, NOEC],
            0x94: [self.sty, ZEROPAGE_X, 2, 4, NOEC],
            0x8c: [self.sty, ABSOLUTE, 3, 4, NOEC],


            ## TRANSFER
            0xaa: [lambda : self.tr(REG_A, REG_X), IMPLICIT, 1, 2, NOEC], # TAX
            0xa8: [lambda : self.tr(REG_A, REG_Y), IMPLICIT, 1, 2, NOEC], # TAY
            0xba: [lambda : self.tr(REG_S, REG_X), IMPLICIT, 1, 2, NOEC], # TSX
            0x8a: [lambda : self.tr(REG_X, REG_A), IMPLICIT, 1, 2, NOEC], # TXA
            0x9a: [lambda : self.tr(REG_X, REG_S), IMPLICIT, 1, 2, NOEC], # TXS
            0x98: [lambda : self.tr(REG_Y, REG_A), IMPLICIT, 1, 2, NOEC], # TYA


            ## COMPARE
            0xc9: [self.cpa, IMMEDIATE, 2, 2, NOEC],
            0xc5: [self.cpa, ZEROPAGE, 2, 3, NOEC],
            0xd5: [self.cpa, ZEROPAGE_X, 2, 4, NOEC],
            0xcd: [self.cpa, ABSOLUTE, 3, 4, NOEC],
            0xdd: [self.cpa, ABSOLUTE_X, 3, 4, YESEC],
            0xd9: [self.cpa, ABSOLUTE_Y, 3, 4, YESEC],
            0xc1: [self.cpa, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0xd1: [self.cpa, POST_INDEX_INDIRECT, 2, 5, YESEC],

            0xe0: [self.cpx, IMMEDIATE, 2, 2, NOEC],
            0xe4: [self.cpx, ZEROPAGE, 2, 3, NOEC],
            0xec: [self.cpx, ABSOLUTE, 3, 4, NOEC],

            0xc0: [self.cpy, IMMEDIATE, 2, 2, NOEC],
            0xc4: [self.cpy, ZEROPAGE, 2, 3, NOEC],
            0xcc: [self.cpy, ABSOLUTE, 3, 4, NOEC],


            ## STACK PUSH/PULL
            0x48: [lambda : self.ph(REG_A), IMPLICIT, 1, 3, NOEC], # PHA
            0x68: [lambda : self.pl(REG_A), IMPLICIT, 1, 4, NOEC], # PLA
            0x08: [lambda : self.ph(REG_S), IMPLICIT, 1, 3, NOEC], # PHP
            0x28: [lambda : self.pl(REG_S), IMPLICIT, 1, 4, NOEC], # PLP


            ## INCREASE / DECREASE
            0xca: [self.dex, IMPLICIT, 1, 2, NOEC], #DEX
            0x88: [self.dey, IMPLICIT, 1, 2, NOEC], #DEY

            0xe8: [self.inx, IMPLICIT, 1, 2, NOEC], # INX
            0xc8: [self.iny, IMPLICIT, 1, 2, NOEC], # INY

            0xc6: [self.dec, ZEROPAGE, 2, 5, NOEC], # DEC
            0xd6: [self.dec, ZEROPAGE_X, 2, 6, NOEC], # DEC
            0xce: [self.dec, ABSOLUTE, 3, 6, NOEC], # DEC
            0xde: [self.dec, ABSOLUTE_X, 3, 7, NOEC], # DEC

            0xe6: [self.inc, ZEROPAGE, 2, 5, NOEC], # INC
            0xf6: [self.inc, ZEROPAGE_X, 2, 6, NOEC], # INC
            0xee: [self.inc, ABSOLUTE, 3, 6, NOEC], # INC
            0xfe: [self.inc, ABSOLUTE_X, 3, 7, NOEC], # INC


            ## BRANCH
            0xd0: [lambda : self.branch(STATUS_ZERO, True), IMPLICIT, 2, 2, BRANCHEC], # BNE
            0xf0: [lambda : self.branch(STATUS_ZERO, False), IMPLICIT, 2, 2, BRANCHEC], # BEQ (inv of BNE)
            0x90: [lambda : self.branch(STATUS_CARRY, True), IMPLICIT, 2, 2, BRANCHEC], # BCC Branch Carry Clear
            0xb0: [lambda : self.branch(STATUS_CARRY, False), IMPLICIT, 2, 2, BRANCHEC], # BCS Branch Carry Set
            0x30: [lambda : self.branch(STATUS_NEG, False), IMPLICIT, 2, 2, BRANCHEC], # BMI Branch on result is negative (mi = minus)
            0x10: [lambda : self.branch(STATUS_NEG, True), IMPLICIT, 2, 2, BRANCHEC], # BPL Branch on result is positive (pl = plsu)
            0x50: [lambda : self.branch(STATUS_OVFLO, True), IMPLICIT, 2, 2, BRANCHEC], # BVC Branch on overflow clear
            0x70: [lambda : self.branch(STATUS_OVFLO, False), IMPLICIT, 2, 2, BRANCHEC], # BVS Branch on overflow set


            ## JUMP
            # 0 byte shift because we handle the shift in the function call
            0x4c: [self.jmp, ABSOLUTE, 0, 3, NOEC], # JMP
            0x6c: [self.jmp, INDIRECT, 0, 5, NOEC], # JMP
            0x20: [self.jsr, ABSOLUTE, 0, 6, NOEC], # JSR
            0x60: [self.rts, IMPLICIT, 0, 6, NOEC], # RTS
        }

        self.check_opcode_map()

    def check_opcode_map(self):
        seen_opcodes = []
        for key, val in self.opcodes.items():
            assert key not in seen_opcodes
            assert len(val) == 5
            seen_opcodes.append(key)
            a,b,c,d,e = val
            if e == YESEC:
                assert b in (ABSOLUTE_X, ABSOLUTE_Y, POST_INDEX_INDIRECT)

    def set_status_bit(self, status_bit, on):
        if on:
            self.regs[REG_S] |= status_bit
        else:
            self.regs[REG_S] &= byte_not(status_bit)

    def get_addr_val(self, mode):
        page_crossed = False
        
        if mode in (ABSOLUTE, ABSOLUTE_X, ABSOLUTE_Y):
            addr = self.mem[self.prgm_ctr+1] + self.mem[self.prgm_ctr+2] * 256
            base_page_no = addr >> 8 # high byte is page no
            if mode == ABSOLUTE_X:
                addr += self.regs[REG_X]
                addr %= 65536
            elif mode == ABSOLUTE_Y:
                addr += self.regs[REG_Y]
                addr %= 65536
            new_page_no = addr >> 8
            # page is crossed if page number has changed
            page_crossed = (new_page_no != base_page_no)

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
            implicit_addr, _, _ = self.get_addr_val(ABSOLUTE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256

        elif mode == PRE_INDEX_INDIRECT:
            # or indexed indirect
            # implicit addr is a ZEROPAGE_X
            implicit_addr, _, _ = self.get_addr_val(ZEROPAGE_X)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256

        elif mode == POST_INDEX_INDIRECT:
            # or indirect indexed
            # implicit addr is a ZEROPAGE
            # the address we found there is offset by REG_Y
            implicit_addr, _, _ = self.get_addr_val(ZEROPAGE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + dest_addr_msb * 256
            base_page_no = addr >> 8
            addr += self.regs[REG_Y]
            addr %= 65536
            new_page_no = addr >> 8
            page_crossed = (new_page_no != base_page_no)
            
        elif mode == ACCUMULATOR:
            val = self.regs[REG_A]
            return None, val, False

        elif mode == IMMEDIATE:
            val = self.mem[self.prgm_ctr+1]
            return None, val, False

        else:
            # relative addressing is only used for branching
            # implicit addressing is operation dependent
            raise Exception
        
        return addr, self.mem[addr], page_crossed
    
    def update_zn_flag(self, value):
        self.set_status_bit(STATUS_ZERO, value == 0)
        self.set_status_bit(STATUS_NEG, value & 0b10000000 != 0)

    def clear(self, status_bit):
        self.set_status_bit(status_bit, False)
    
    def jmp(self, addr, val):
        self.prgm_ctr = addr
            
    def stack_push(self, val):
        stack_addr = self.stack_ptr + 0x0100
        self.mem[stack_addr] = val
        self.stack_ptr -= 1

    def stack_pull(self):
        self.stack_ptr += 1
        stack_addr = self.stack_ptr + 0x0100
        return self.mem[stack_addr]

    def jsr(self, addr, val):
        self.stack_push(self.prgm_ctr + 2)
        self.jmp(addr, val)

    def rts(self):
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

    def bit(self, addr, val):
        # https://www.masswerk.at/6502/6502_instruction_set.html#bitcompare
        acc = self.regs[REG_A]
        self.set_status_bit(STATUS_ZERO, acc & val == 0)
        self.set_status_bit(STATUS_NEG, val & 0b10000000 != 0)
        self.set_status_bit(STATUS_OVFLO, val & 0b01000000 != 0)

    def ld(self, reg, val):
        # load accumulator
        self.regs[reg] = val
        self.update_zn_flag(val)

    def lda(self, addr, val):
        return self.ld(REG_A, val)

    def ldx(self, addr, val):
        return self.ld(REG_X, val)
    
    def ldy(self, addr, val):
        return self.ld(REG_Y, val)
    
    def st(self, reg, addr):
        val = self.regs[reg]
        self.mem[addr] = val
        self.update_zn_flag(val)

    def sta(self, addr, val):
        return self.st(REG_A, addr)   
    
    def stx(self, addr, val):
        return self.st(REG_X, addr)
    
    def sty(self, addr, val):
        return self.st(REG_Y, addr)
    
    def tr(self, sreg, dreg):
        val = self.regs[sreg]
        self.regs[dreg] = val
        self.update_zn_flag(val)
        
    def cp(self, reg, val):
        diff = self.regs[reg] - val
        self.update_zn_flag(diff) #status_zero goes to 0 if equality

    def cpa(self, addr, val):
        return self.cp(REG_A, val)
    
    def cpx(self, addr, val):
        return self.cp(REG_X, val)
    
    def cpy(self, addr, val):
        return self.cp(REG_Y, val)

    def in_de_reg(self, reg, sign_plus):
        # increase or decrease handler
        if sign_plus:
            self.regs[reg] += 1
        else:
            self.regs[reg] -= 1
        self.regs[reg] %= 256
        self.update_zn_flag(self.regs[reg])

    def inx(self):
        return self.in_de_reg(REG_X, True)

    def dex(self):
        return self.in_de_reg(REG_X, False)
    
    def iny(self):
        return self.in_de_reg(REG_Y, True)
    
    def dey(self):
        return self.in_de_reg(REG_Y, False)
    
    def in_de_mem(self, addr, sign_plus):
        if sign_plus:
            self.mem[addr] += 1
        else:
            self.mem[addr] -= 1
        self.mem[addr] %= 256
        self.update_zn_flag(self.mem[addr])

    def inc(self, addr, val):
        return self.in_de_mem(addr, True)
    
    def dec(self, addr, val):
        return self.in_de_mem(addr, False)

    def add_val_to_acc_carry(self, val):
        if self.regs[REG_S] & STATUS_CARRY != 0:
            # there is a carry
            val += 1
        self.regs[REG_A] += val
        self.set_status_bit(STATUS_CARRY, self.regs[REG_A] > 255)
        self.regs[REG_A] %= 256
        self.update_zn_flag(self.regs[REG_A])

    def adc(self, addr, val):
        self.add_val_to_acc_carry(val)

    def sbc(self, addr, val):
        # use two's complement https://stackoverflow.com/a/41253661
        self.add_val_to_acc_carry(byte_not(val))

    def and_(self, addr, val):
        self.regs[REG_A] &= val
        self.update_zn_flag(self.regs[REG_A])

    def or_(self, addr, val):
        self.regs[REG_A] |= val
        self.update_zn_flag(self.regs[REG_A])

    def branch(self, status_bit, branch_if_zero):
        extra_cycles = 0
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
            base_page = self.prgm_ctr >> 8
            self.prgm_ctr += branch_addr
            self.prgm_ctr %= 65536
            new_page = self.prgm_ctr >> 8
            if base_page == new_page:
                extra_cycles = 1
            else:
                extra_cycles = 2
        return extra_cycles

    def shift_right_memory(self, addr, val):
        # carry if lsb set
        carry_on = (self.mem[addr] & 0b00000001 != 0)
        self.mem[addr] >>= 1
        self.update_zn_flag(self.mem[addr])
        self.set_status_bit(STATUS_CARRY, carry_on)

    def shift_right_accumulator(self):
        # carry if lsb set
        carry_on = (self.regs[REG_A] & 0b00000001 != 0)
        self.regs[REG_A] >>= 1
        self.update_zn_flag(self.regs[REG_A])
        self.set_status_bit(STATUS_CARRY, carry_on)

    def dbg(self):
        print()
        inst = self.lst.get_inst(self.prgm_ctr)
        print(inst)
        print("PC :", hex(self.prgm_ctr),
              "\tinst :", hex(self.mem[self.prgm_ctr]),
              "\tA :", hex(self.regs[REG_A]),
              "\tX :", hex(self.regs[REG_X]),
              "\tY :", hex(self.regs[REG_Y]),
              "\tS :", bin(self.regs[REG_S]),
              "\tSP :", hex(self.stack_ptr))
        print("mem: ", *[dec2hex(i) for i in self.mem[0x00:0x10]])
        if "bkpt" in inst:
            input()

    def display_mem(self):
        # 32 x 32 from 0x0200 to 0x05ff
        mat = np.array(self.mem[0x0200:0x0600]).reshape((32,32))
        plt.imshow(mat)
        plt.show()

    def get_display_mat(self):
        mat = np.array(self.mem[0x0200:0x0600]).reshape((32,32))
        return mat

    def exec_inst(self):
        e.mem[0xfe] = int(random.random()*256) # RANDOM GEN

        opcode = self.mem[self.prgm_ctr]
        if opcode == 0x00:
            # TODO : handle BRK in a better way (raise interrupt flag)
            return -1
        if not opcode in self.opcodes:
            raise Exception(f"Unknown opcode {hex(opcode)}")
        func, addr_mode, nbytes, base_ncycle, extra_cycle_type = self.opcodes[opcode]
        extra_cycle_nb = 0
        if extra_cycle_type == BRANCHEC:
            # special case where "branch" returns the number of extra cycles
            extra_cycle_nb = func()
        else:
            if addr_mode in (IMPLICIT, ACCUMULATOR):
                # functions with no arg
                func()
            else:
                # functions that uses addressing
                addr, val, page_crossed = self.get_addr_val(addr_mode)
                func(addr, val)
                if page_crossed:
                    extra_cycle_nb = 1
        ncycle = base_ncycle + extra_cycle_nb
        # self.dbg()
        self.prgm_ctr += nbytes
        time.sleep(0.000001)
        return ncycle

    def tick(self):
        # run instruction if we are at the beggining of the cycle
        # else just register the tick
        if self.instuction_cycle == 0:
            self.instruction_nbcycles = self.exec_inst()
            if self.instruction_nbcycles == -1:
                return False
        self.instuction_cycle += 1
        if self.instuction_cycle == self.instruction_nbcycles:
            self.instuction_cycle = 0
        return True

    def run(self):
        while self.tick():
            pass


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
    
class AnimatedImshow:
    def __init__(self, data_provider, interval=50):
        """
        Initialize the AnimatedImshow class.

        Parameters:
        - data_provider: An instance of DataProvider.
        - interval: The delay between frames in milliseconds.
        - cmap: The colormap to use for the imshow.
        """
        self.data_provider = data_provider
        self.interval = interval
        self.fig, self.ax = plt.subplots()
        self.im = self.ax.imshow(np.random.rand(32,32)) # if i don't use a random init vector it don't work???

    def update(self, frame):
        """
        Update the image for the given frame.

        Parameters:
        - frame: The current frame index.
        """
        mat = self.data_provider.get_display_mat()
        self.im.set_array(mat)
        return [self.im]

    def animate(self):
        """
        Create and display the animation.
        """
        self.ani = animation.FuncAnimation(
            self.fig, self.update, interval=self.interval, blit=True)
        plt.show()


l = LstManager()

e = Emulator(l)
m = AnimatedImshow(e)

import evdev
dev = evdev.InputDevice('/dev/input/event4')

print(dev)

def readkb():
    for event in dev.read_loop():
        if event.type == evdev.ecodes.EV_KEY:

            key = evdev.ecodes.KEY[event.code][4:]
            if len(key) != 1:
                continue
            if event.value != 1: #press down
                continue
            asciival = ord(key.lower())
            e.mem[0xff] = asciival

kbthread = threading.Thread(target=readkb)
kbthread.start()


e.start()
m.animate()
kbthread.join()
