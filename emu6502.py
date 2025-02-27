import threading
import time

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
STATUS_BIT5  = 0b00100000
STATUS_BREAK = 0b00010000
STATUS_DEC   = 0b00001000
STATUS_INTER = 0b00000100
STATUS_ZERO  = 0b00000010
STATUS_CARRY = 0b00000001

NOEC = 0
YESEC = 1
BRANCHEC = 2

INTERRUPT_NO = 0 # No interrupt
INTERRUPT_IRQ = 1 # Interrupt ReQuest
INTERRUPT_NMI = 2 # Non maskable interrupt
INTERRUPT_RST = 3 # Reset (not actually an interrupt but it is handy to handle it this way)

OPCODE_RST = 0xffd
OPCODE_IRQ = 0xffe
OPCODE_NMI = 0xfff

def byte_not(val):
    return ~val%256

def dec2hex(val):
    val = hex(val)[2:]
    if len(val) == 1:
        val = "0" + val
    return val

def low_byte(val):
    return val % 256

def high_byte(val):
    return val // 256

def bin8(val):
    s = bin(val)[2:]
    return '0b' + '0'*max(0, (8-len(s))) + s

def hex2(val):
    s = hex(val)[2:]
    return '0x' + '0'*max(0,(2-len(s))) + s

class Memory:
    def __init__(self, memory_map):
        """
        Memory map is a list of tuples
        (startaddr, device)
        From lowest startaddr to greatest
        """
        self.mmap = memory_map
        # reverse the list to ease device search
        # search from the biggest addr and stop at the
        # first one lowest than the addr were looking for
        self.mmap.reverse()

    def address_resolve(self, cpuaddr):
        for startaddr, device in self.mmap:
            if cpuaddr >= startaddr:
                return cpuaddr - startaddr, device
        raise Exception("Bad memory map (couldn't find stuitable device for requested addr)")

    def __getitem__(self, index):
        if type(index) == slice:
            dev_addr, dev = self.address_resolve(index.start)
            dev_addr2, dev2 = self.address_resolve(index.stop - 1)
            if dev != dev2:
                raise Exception("The used slice overlaps several devices")
            return dev[dev_addr:dev_addr2 + 1]
        else:
            dev_addr, dev = self.address_resolve(index)
        return dev[dev_addr]

    def __setitem__(self, index, value):
        if type(index) == slice:
            raise Exception("Memory setitem don't support slices")
        dev_addr, dev = self.address_resolve(index)
        dev[dev_addr] = value

class Emu6502(threading.Thread):
    def __init__(self, memory_map, lst = None, debug = False):
        super().__init__()
        self.debug = debug
        self.lst = lst

        self.regs = [0,0,0,0]
        self.stack_ptr = 0xff

        # Currently unset, will be set with the power-on reset
        self.prgm_ctr = None

        # if no HW interrupt (understand : no external interrupt)
        # set to INTERRUPT_NO
        # else, it will save the interrupt type
        self.interrupt_type = INTERRUPT_RST 

        self.mem = Memory(memory_map)

        self.instuction_cycle = 0
        self.instruction_nbcycles = 0

        # operation, nbytes, ncycles, extracycles
        self.opcodes = {
            ## INTERRUPTS
            # fake opcode to handle hw interrupts
            OPCODE_IRQ : [self.irq, IMPLICIT, 0, 7, NOEC],
            OPCODE_NMI : [self.nmi, IMPLICIT, 0, 7, NOEC],
            OPCODE_RST : [self.reset, IMPLICIT, 0, 7, NOEC], # TODO : Check if reset is actually 7 cycles long

            0x00: [self.brk, IMPLICIT, 0, 7, NOEC], # BRK : Like a jump, nbytes=0
            0x40: [self.rti, IMPLICIT, 0, 6, NOEC],

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


            ## EOR
            0x49: [self.eor, IMMEDIATE, 2, 2, NOEC],
            0x45: [self.eor, ZEROPAGE, 2, 3, NOEC], 
            0x55: [self.eor, ZEROPAGE_X, 2, 4, NOEC],
            0x4D: [self.eor, ABSOLUTE, 3, 4, NOEC],
            0x5D: [self.eor, ABSOLUTE_X, 3, 4, YESEC],
            0x59: [self.eor, ABSOLUTE_Y, 3, 4, YESEC],
            0x41: [self.eor, PRE_INDEX_INDIRECT, 2, 6, NOEC],
            0x51: [self.eor, POST_INDEX_INDIRECT, 2, 5, YESEC],


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
            0x4a: [self.func_with_acc(self.shift_right), ACCUMULATOR, 1, 2, NOEC],
            0x46: [self.func_with_mem(self.shift_right), ZEROPAGE, 2, 5, NOEC],
            0x56: [self.func_with_mem(self.shift_right), ZEROPAGE_X, 2, 6, NOEC],
            0x4e: [self.func_with_mem(self.shift_right), ABSOLUTE, 3, 6, NOEC],
            0x5e: [self.func_with_mem(self.shift_right), ABSOLUTE_X, 3, 7, NOEC],

            # ASL
            0x0a: [self.func_with_acc(self.shift_left), ACCUMULATOR, 1, 2, NOEC],
            0x06: [self.func_with_mem(self.shift_left), ZEROPAGE, 2, 5, NOEC],
            0x16: [self.func_with_mem(self.shift_left), ZEROPAGE_X, 2, 6, NOEC],
            0x0e: [self.func_with_mem(self.shift_left), ABSOLUTE, 3, 6, NOEC],
            0x1e: [self.func_with_mem(self.shift_left), ABSOLUTE_X, 3, 7, NOEC],

            # ROL
            0x2a: [self.func_with_acc(self.rotate_left), ACCUMULATOR, 1, 2, NOEC],
            0x26: [self.func_with_mem(self.rotate_left), ZEROPAGE, 2, 5, NOEC],
            0x36: [self.func_with_mem(self.rotate_left), ZEROPAGE_X, 2, 6, NOEC],
            0x2e: [self.func_with_mem(self.rotate_left), ABSOLUTE, 3, 6, NOEC],
            0x3e: [self.func_with_mem(self.rotate_left), ABSOLUTE_X, 3, 7, NOEC],

            # ROR
            0x6a: [self.func_with_acc(self.rotate_left), ACCUMULATOR, 1, 2, NOEC],
            0x66: [self.func_with_mem(self.rotate_left), ZEROPAGE, 2, 5, NOEC],
            0x76: [self.func_with_mem(self.rotate_left), ZEROPAGE_X, 2, 6, NOEC],
            0x6e: [self.func_with_mem(self.rotate_left), ABSOLUTE, 3, 6, NOEC],
            0x7e: [self.func_with_mem(self.rotate_left), ABSOLUTE_X, 3, 7, NOEC],


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

    def get_status_bit(self, status_bit):
        if self.regs[REG_S] & status_bit != 0:
            return 1
        return 0

    def get_addr(self, mode):
        page_crossed = False
        
        if mode in (ABSOLUTE, ABSOLUTE_X, ABSOLUTE_Y):
            addr = self.mem[self.prgm_ctr+1] + self.mem[self.prgm_ctr+2] * 256
            base_page_no = high_byte(addr) # high byte is page no
            if mode == ABSOLUTE_X:
                addr += self.regs[REG_X]
                addr %= 65536
            elif mode == ABSOLUTE_Y:
                addr += self.regs[REG_Y]
                addr %= 65536
            new_page_no = high_byte(addr)
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
            implicit_addr, _ = self.get_addr(ABSOLUTE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + (dest_addr_msb << 8)

        elif mode == PRE_INDEX_INDIRECT:
            # or indexed indirect
            # implicit addr is a ZEROPAGE_X
            implicit_addr, _ = self.get_addr(ZEROPAGE_X)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + (dest_addr_msb << 8)

        elif mode == POST_INDEX_INDIRECT:
            # or indirect indexed
            # implicit addr is a ZEROPAGE
            # the address we found there is offset by REG_Y
            implicit_addr, _ = self.get_addr(ZEROPAGE)
            dest_addr_lsb = self.mem[implicit_addr]
            dest_addr_msb = self.mem[(implicit_addr + 1)%65536]
            addr = dest_addr_lsb + (dest_addr_msb << 8)
            base_page_no = high_byte(addr)
            addr += self.regs[REG_Y]
            addr %= 65536
            new_page_no = high_byte(addr)
            page_crossed = (new_page_no != base_page_no)

        elif mode == IMMEDIATE:
            # the actual address for immediate addr is next value
            return self.prgm_ctr+1, False

        else:
            # relative addressing is only used for branching
            # implicit addressing is operation dependent
            # "accumulator" special addressing is handled separatly
            raise Exception
        
        return addr, page_crossed
    
    def update_zn_flag(self, value):
        self.set_status_bit(STATUS_ZERO, value == 0)
        self.set_status_bit(STATUS_NEG, (value & 0b10000000) != 0)

    def clear(self, status_bit):
        self.set_status_bit(status_bit, False)
    
    def jmp(self, addr):
        self.prgm_ctr = addr
            
    def stack_push(self, val):
        stack_addr = self.stack_ptr + 0x0100
        self.mem[stack_addr] = val
        self.stack_ptr -= 1

    def stack_pull(self):
        self.stack_ptr += 1
        stack_addr = self.stack_ptr + 0x0100
        return self.mem[stack_addr]

    def jsr(self, addr):
        self.stack_push(self.prgm_ctr + 2)
        self.jmp(addr)

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

    def bit(self, addr):
        # https://www.masswerk.at/6502/6502_instruction_set.html#bitcompare
        acc = self.regs[REG_A]
        val = self.mem[addr]
        self.set_status_bit(STATUS_ZERO, acc & val == 0)
        self.set_status_bit(STATUS_NEG, val & 0b10000000 != 0)
        self.set_status_bit(STATUS_OVFLO, val & 0b01000000 != 0)

    def load(self, reg, val):
        # load accumulator
        self.regs[reg] = val
        self.update_zn_flag(val)

    def lda(self, addr):
        return self.load(REG_A, self.mem[addr])

    def ldx(self, addr):
        return self.load(REG_X, self.mem[addr])
    
    def ldy(self, addr):
        return self.load(REG_Y, self.mem[addr])
    
    def store(self, reg, addr):
        val = self.regs[reg]
        self.mem[addr] = val
        self.update_zn_flag(val)

    def sta(self, addr):
        return self.store(REG_A, addr)   
    
    def stx(self, addr):
        return self.store(REG_X, addr)
    
    def sty(self, addr):
        return self.store(REG_Y, addr)
    
    def tr(self, sreg, dreg):
        val = self.regs[sreg]
        self.regs[dreg] = val
        self.update_zn_flag(val)
        
    def cp(self, reg, val):
        is_carry = (self.regs[reg] + byte_not(val) + 1) > 255
        diff = self.regs[reg] - val
        self.set_status_bit(STATUS_CARRY, is_carry)
        self.update_zn_flag(diff) #status_zero goes to 0 if equality

    def cpa(self, addr):
        return self.cp(REG_A, self.mem[addr])
    
    def cpx(self, addr):
        return self.cp(REG_X, self.mem[addr])
    
    def cpy(self, addr):
        return self.cp(REG_Y, self.mem[addr])

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
        val = self.mem[addr]
        if sign_plus:
            val += 1
        else:
            val -= 1
        val %= 256
        self.mem[addr] = val
        self.update_zn_flag(val)

    def inc(self, addr):
        return self.in_de_mem(addr, True)
    
    def dec(self, addr):
        return self.in_de_mem(addr, False)

    def add_val_to_acc_carry(self, val):
        if self.get_status_bit(STATUS_CARRY) == 1:
            # there is a carry
            val += 1
        self.regs[REG_A] += val
        self.set_status_bit(STATUS_CARRY, self.regs[REG_A] > 255)
        self.regs[REG_A] %= 256
        self.update_zn_flag(self.regs[REG_A])

    def adc(self, addr):
        self.add_val_to_acc_carry(self.mem[addr])

    def sbc(self, addr):
        # use two's complement https://stackoverflow.com/a/41253661
        self.add_val_to_acc_carry(byte_not(self.mem[addr]))

    def and_(self, addr):
        self.regs[REG_A] &= self.mem[addr]
        self.update_zn_flag(self.regs[REG_A])

    def or_(self, addr):
        self.regs[REG_A] |= self.mem[addr]
        self.update_zn_flag(self.regs[REG_A])

    def eor(self, addr):
        self.regs[REG_A] ^= self.mem[addr]
        self.update_zn_flag(self.regs[REG_A])

    def branch(self, status_bit, branch_if_zero):
        extra_cycles = 0
        do_branch = False
        if self.get_status_bit(status_bit) == 0:
            do_branch = True
        if not branch_if_zero:
            do_branch = not do_branch
        if do_branch:
            # branch
            branch_addr = self.mem[self.prgm_ctr + 1]
            if branch_addr & 0b10000000 != 0:
                # negative number
                branch_addr -= 256
            base_page = high_byte(self.prgm_ctr)
            self.prgm_ctr += branch_addr
            self.prgm_ctr %= 65536
            new_page = high_byte(self.prgm_ctr)
            if base_page == new_page:
                extra_cycles = 1
            else:
                extra_cycles = 2
        return extra_cycles

    def func_with_acc(self, func):
        """
        wrapper to run a function on the accumulator
        """
        def f():
            self.regs[REG_A] = func(self.regs[REG_A])
        return f
    
    def func_with_mem(self, func):
        """
        wrapper to run a function on the memory
        """
        def f(addr):
            self.mem[addr] = func(self.mem[addr])
        return f

    def shift_right(self, val):
        # carry if lsb set
        carry_on = (val & 0b00000001 != 0)
        val >>= 1
        self.update_zn_flag(val)
        self.set_status_bit(STATUS_CARRY, carry_on)
        return val

    def shift_left(self, val):
        # carry if lsb set
        carry_on = (val & 0b10000000 != 0)
        val <<= 1
        val &= 0b11111111 #truncate to 8 bits
        self.update_zn_flag(val)
        self.set_status_bit(STATUS_CARRY, carry_on)
        return val

    def rotate_right(self, val):
        next_carry = self.get_status_bit(STATUS_CARRY)
        next_carry = int((val & 0b00000001) != 0)
        val >>= 1
        val += next_carry << 7
        self.update_zn_flag(val)
        self.set_status_bit(STATUS_CARRY, bool(next_carry))
        return val

    def rotate_left(self, val):
        curr_carry = self.get_status_bit(STATUS_CARRY)
        next_carry = int((val & 0b10000000) != 0)
        val <<= 1
        val &= 0b11111111 #truncate to 8 bits
        val += curr_carry
        self.update_zn_flag(val)
        self.set_status_bit(STATUS_CARRY, bool(next_carry))
        return val

    def hw_interrupt(self, maskable):
        """
        Maskable = true : IRQ
        Maskable = false : NMI
        """

        # don't do anything if IRQ and IRQ has been disabled
        if maskable and self.get_status_bit(STATUS_INTER) == 1:
            return
        
        self.stack_push(high_byte(self.prgm_ctr))
        self.stack_push(low_byte(self.prgm_ctr))
        self.stack_push(self.regs[REG_S])
        prgm_ctr_addr = 0xfffa
        if maskable:
            prgm_ctr_addr = 0xfffe
        self.prgm_ctr = (self.mem[prgm_ctr_addr+1] << 8) + self.mem[prgm_ctr_addr]

    def nmi(self):
        self.set_status_bit(STATUS_BREAK, False)
        return self.hw_interrupt(False)
    
    def irq(self):
        self.set_status_bit(STATUS_BREAK, False)
        return self.hw_interrupt(True)
    
    def brk(self):
        """
        Break is like an NMI, but instead to store PC in stack we store PC+2
        So we increade PC by 2 and fwd to hw interrupt
        """
        self.set_status_bit(STATUS_BREAK, True)
        self.prgm_ctr += 2
        self.hw_interrupt(maskable=False)

    def rti(self):
        old_status = self.stack_pull()
        curr_status = self.regs[REG_S]

        # we want to keep the same value for bit 4 (break) and 5
        status_ignore_mask = STATUS_BREAK & STATUS_BIT5
        status_ignore_mask_bar = byte_not(status_ignore_mask)
        # set to 1 the ignored bits
        old_status |= status_ignore_mask
        # set to 1 the unignored bits
        curr_status |= status_ignore_mask_bar

        self.regs[REG_S] = old_status & curr_status # = 0bxx11xxxx & 0b11yy1111 = 0bxxyyxxxx

        self.set_status_bit(STATUS_BREAK, False)
        self.set_status_bit(STATUS_BIT5, False)

        pc_low = self.stack_pull()
        pc_high = self.stack_pull()
        self.prgm_ctr = (pc_high << 8) + pc_low


    def dbg(self):
        if self.prgm_ctr is None:
            return
        print()
        print("PC\tinst\tA\tX\tY\tSP\t  NV-BDIZC")
        print(
            hex(self.prgm_ctr),
            hex2(self.mem[self.prgm_ctr]),
            hex2(self.regs[REG_A]),
            hex2(self.regs[REG_X]),
            hex2(self.regs[REG_Y]),
            hex2(self.stack_ptr),
            bin8(self.regs[REG_S]),
            sep="\t"
        )
        if self.lst is not None:
            inst = self.lst.get_inst(self.prgm_ctr)
            print(inst)
            if "bkpt" in inst:
                time.sleep(2)

    def interrupt(self, maskable):
        """
        This will set the interrupt type, causing the trigger of an
        hw interrupt at the next op execution
        Made to be called externally
        """
        if maskable:
            self.interrupt_type = INTERRUPT_IRQ
        else:
            self.interrupt_type = INTERRUPT_NMI

    def reset(self):
        reset_vector = 0xfffc
        self.prgm_ctr = (self.mem[reset_vector+1] << 8) + self.mem[reset_vector]

    def exec_inst(self):
        if self.debug:
            self.dbg()

        opcode = None
        if self.interrupt_type != INTERRUPT_NO:
            # hw interrupt is requested
            # retreive the fake opcode to run the instruct
            # as if it was any other function
            if self.interrupt_type == INTERRUPT_IRQ:
                opcode = OPCODE_IRQ
            elif self.interrupt_type == INTERRUPT_NMI:
                opcode = OPCODE_NMI
            elif self.interrupt_type == INTERRUPT_RST:
                opcode = OPCODE_RST
            # reset interrupt type
            self.interrupt_type = INTERRUPT_NO
        
        else:
            # no interrupt, run the next intruction normally
            opcode = self.mem[self.prgm_ctr]

        if opcode is None or opcode not in self.opcodes:
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
                addr, page_crossed = self.get_addr(addr_mode)
                func(addr)
                if page_crossed:
                    extra_cycle_nb = 1
        ncycle = base_ncycle + extra_cycle_nb
        self.prgm_ctr += nbytes
        # input()
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
