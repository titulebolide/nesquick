#pragma once

#include <vector>
#include <map>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iomanip>
#include <bitset>
#include <thread>
#include <chrono>

#include "cpumem.hpp"
#include "lstdebugger.hpp"

// TODO : use enums instead...
// Constants for registers
const int REG_A = 0;
const int REG_X = 1;
const int REG_Y = 2;
const int REG_S = 3; // status register

// Addressing modes
const int IMMEDIATE = 0;
const int ZEROPAGE = 1;
const int ZEROPAGE_X = 2;
const int ZEROPAGE_Y = 3;
const int ABSOLUTE = 4;
const int ABSOLUTE_X = 5;
const int ABSOLUTE_Y = 6;
const int INDIRECT = 8;
const int PRE_INDEX_INDIRECT = 9;
const int POST_INDEX_INDIRECT = 10;
const int ACCUMULATOR = 11;
const int IMPLICIT = 12;

// Status register bits
const uint8_t STATUS_NEG   = 0b10000000;
const uint8_t STATUS_OVFLO = 0b01000000;
const uint8_t STATUS_BIT5  = 0b00100000;
const uint8_t STATUS_BREAK = 0b00010000;
const uint8_t STATUS_DEC   = 0b00001000;
const uint8_t STATUS_INTER = 0b00000100;
const uint8_t STATUS_ZERO  = 0b00000010;
const uint8_t STATUS_CARRY = 0b00000001;

// Extra cycle types
const int NOEC = 0;
const int YESEC = 1;
const int BRANCHEC = 2;

// Interrupt types
const int INTERRUPT_NO = 0; // No interrupt
const int INTERRUPT_IRQ = 1; // Interrupt ReQuest
const int INTERRUPT_NMI = 2; // Non maskable interrupt
const int INTERRUPT_RST = 3; // Reset

// Opcodes for interrupts
const uint16_t OPCODE_RST = 0xffd;
const uint16_t OPCODE_IRQ = 0xffe;
const uint16_t OPCODE_NMI = 0xfff;

class Emu6502 {
public:
    Emu6502(Memory *mem, bool debug = false, LstDebuggerAsm6 *lst = nullptr);
    void interrupt(bool maskable);
    void op_reset();
    bool tick();
    void setDebug(bool debug);

private:
    void set_status_bit(uint8_t status_bit, bool on);
    bool get_status_bit(uint8_t status_bit);
    void update_zn_flag(uint8_t value);
    void ph(int reg);
    void pl(int reg);
    void load(int reg, uint8_t val);
    void stack_push(uint8_t val);
    uint8_t stack_pull();
    void store(int reg, uint16_t addr);
    void transfer(int sreg, int dreg);
    void compare(int reg, uint8_t val);
    void in_de_reg(int reg, bool sign_plus);
    void in_de_mem(uint16_t addr, bool sign_plus);
    void add_val_to_acc_carry(uint8_t val);
    void branch(uint8_t status_bit, bool branch_if_zero);
    uint8_t shift_right(uint8_t val);
    uint8_t shift_left(uint8_t val);
    uint8_t rotate_right(uint8_t val);
    uint8_t rotate_left(uint8_t val);
    void hw_interrupt(bool maskable);
    void dbg();
    int exec_inst();
    
    // op functions
    void op_nmi();
    void op_irq();
    void op_brk();
    void op_rti();

    void op_jsr();
    void op_jmp();
    void op_rts();
    void op_bit();

    void op_lda() { load(REG_A, mem->get(op_addr)); }
    void op_ldx() { load(REG_X, mem->get(op_addr)); }
    void op_ldy() { load(REG_Y, mem->get(op_addr)); }

    void op_cpa() { compare(REG_A, mem->get(op_addr)); }
    void op_cpx() { compare(REG_X, mem->get(op_addr)); }
    void op_cpy() { compare(REG_Y, mem->get(op_addr)); }

    void op_sta() { store(REG_A, op_addr); }
    void op_stx() { store(REG_X, op_addr); }
    void op_sty() { store(REG_Y, op_addr); }

    void op_inx();
    void op_dex();
    void op_iny();
    void op_dey();
    void op_inc() { in_de_mem(op_addr, true); }
    void op_dec() { in_de_mem(op_addr, false); }
    
    void op_adc();
    void op_sbc();
    void op_and();
    void op_ora();
    void op_eor();

    void op_clc() { set_status_bit(STATUS_CARRY, false); }
    void op_cld() { set_status_bit(STATUS_DEC, false); }
    void op_cli() { set_status_bit(STATUS_INTER, false); }
    void op_clv() { set_status_bit(STATUS_OVFLO, false); }
    void op_sec() { set_status_bit(STATUS_CARRY, true); }
    void op_sed() { set_status_bit(STATUS_DEC, true); }
    void op_sei() { set_status_bit(STATUS_INTER, true); }

    void op_lsr_acc() { regs[REG_A] = shift_right(regs[REG_A]); }
    void op_asl_acc() { regs[REG_A] = shift_left(regs[REG_A]); }
    void op_ror_acc() { regs[REG_A] = rotate_right(regs[REG_A]); }
    void op_rol_acc() { regs[REG_A] = rotate_left(regs[REG_A]); }
    void op_lsr_mem() { mem->set(op_addr, shift_right(mem->get(op_addr))); }
    void op_asl_mem() { mem->set(op_addr, shift_left(mem->get(op_addr))); }
    void op_ror_mem() { mem->set(op_addr, rotate_right(mem->get(op_addr))); }
    void op_rol_mem() { mem->set(op_addr, rotate_left(mem->get(op_addr))); }

    void op_tax() { transfer(REG_A, REG_X); }
    void op_tay() { transfer(REG_A, REG_Y); }
    void op_tsx() { transfer(REG_S, REG_X); }
    void op_txa() { transfer(REG_X, REG_A); }
    void op_txs() { transfer(REG_X, REG_S); }
    void op_tya() { transfer(REG_Y, REG_A); }

    void op_pha() { ph(REG_A); }
    void op_pla() { pl(REG_A); }
    void op_php() { ph(REG_S); }
    void op_plp() { pl(REG_S); }

    void op_bne() { branch(STATUS_ZERO, true); }
    void op_beq() { branch(STATUS_ZERO, false); }
    void op_bcc() { branch(STATUS_CARRY, true); }
    void op_bcs() { branch(STATUS_CARRY, false); }
    void op_bmi() { branch(STATUS_NEG, false); }
    void op_bpl() { branch(STATUS_NEG, true); }
    void op_bvc() { branch(STATUS_OVFLO, true); }
    void op_bvs() { branch(STATUS_OVFLO, false); }

    void op_nop() {}


private:
    bool m_debug;
    // TODO : either move this to an struct or to a uint8_t regs[4]
    // TODO : all the checks that regs[smth] > 255 are broken because it is an uint8_t
    // TODO : We have to find another way to detect a carry !
    std::vector<uint8_t> regs;
    uint8_t stack_ptr;
    uint16_t prgm_ctr;
    int interrupt_type;
    Memory *mem;
    LstDebuggerAsm6 *lst;
    int instruction_cycle;
    int instruction_nbcycles;

    struct Opcode {
        void (Emu6502::*func)();
        uint addr_mode;
        uint nbytes;
        uint base_ncycle;
        uint extra_cycle_type;
    };

    // used specifically for opcode execution (e.g. for  passing mem addr to some opcodes)
    uint op_extra_cycles;
    uint16_t op_addr;


    void check_opcode_map();
    uint16_t get_addr(int mode, bool *page_crossed);

    std::map<uint16_t, Opcode> opcodes {
        // INTERRUPTS
        {OPCODE_IRQ, {&Emu6502::op_irq, IMPLICIT, 0, 7, NOEC}},
        {OPCODE_NMI, {&Emu6502::op_nmi, IMPLICIT, 0, 7, NOEC}},
        {OPCODE_RST, {&Emu6502::op_reset, IMPLICIT, 0, 7, NOEC}},
    
        // BRK and RTI
        {0x00, {&Emu6502::op_brk, IMPLICIT, 0, 7, NOEC}},
        {0x40, {&Emu6502::op_rti, IMPLICIT, 0, 6, NOEC}},
    
        // NOP
        {0xea, {&Emu6502::op_nop, IMPLICIT, 1, 2, NOEC}},
    
        // BIT TEST
        {0x24, {&Emu6502::op_bit, ZEROPAGE, 2, 3, NOEC}},
        {0x2c, {&Emu6502::op_bit, ABSOLUTE, 3, 4, NOEC}},
    
        // ADC (Add with Carry)
        {0x69, {&Emu6502::op_adc, IMMEDIATE, 2, 2, NOEC}},
        {0x65, {&Emu6502::op_adc, ZEROPAGE, 2, 3, NOEC}},
        {0x75, {&Emu6502::op_adc, ZEROPAGE_X, 2, 4, NOEC}},
        {0x6d, {&Emu6502::op_adc, ABSOLUTE, 3, 4, NOEC}},
        {0x7d, {&Emu6502::op_adc, ABSOLUTE_X, 3, 4, YESEC}},
        {0x79, {&Emu6502::op_adc, ABSOLUTE_Y, 3, 4, YESEC}},
        {0x61, {&Emu6502::op_adc, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0x71, {&Emu6502::op_adc, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // SBC (Subtract with Carry)
        {0xe9, {&Emu6502::op_sbc, IMMEDIATE, 2, 2, NOEC}},
        {0xe5, {&Emu6502::op_sbc, ZEROPAGE, 2, 3, NOEC}},
        {0xf5, {&Emu6502::op_sbc, ZEROPAGE_X, 2, 4, NOEC}},
        {0xed, {&Emu6502::op_sbc, ABSOLUTE, 3, 4, NOEC}},
        {0xfd, {&Emu6502::op_sbc, ABSOLUTE_X, 3, 4, YESEC}},
        {0xf9, {&Emu6502::op_sbc, ABSOLUTE_Y, 3, 4, YESEC}},
        {0xe1, {&Emu6502::op_sbc, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0xf1, {&Emu6502::op_sbc, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // AND (Logical AND)
        {0x29, {&Emu6502::op_and, IMMEDIATE, 2, 2, NOEC}},
        {0x25, {&Emu6502::op_and, ZEROPAGE, 2, 3, NOEC}},
        {0x35, {&Emu6502::op_and, ZEROPAGE_X, 2, 4, NOEC}},
        {0x2d, {&Emu6502::op_and, ABSOLUTE, 3, 4, NOEC}},
        {0x3d, {&Emu6502::op_and, ABSOLUTE_X, 3, 4, YESEC}},
        {0x39, {&Emu6502::op_and, ABSOLUTE_Y, 3, 4, YESEC}},
        {0x21, {&Emu6502::op_and, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0x31, {&Emu6502::op_and, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // ORA (Logical OR)
        {0x09, {&Emu6502::op_ora, IMMEDIATE, 2, 2, NOEC}},
        {0x05, {&Emu6502::op_ora, ZEROPAGE, 2, 3, NOEC}},
        {0x15, {&Emu6502::op_ora, ZEROPAGE_X, 2, 4, NOEC}},
        {0x0d, {&Emu6502::op_ora, ABSOLUTE, 3, 4, NOEC}},
        {0x1d, {&Emu6502::op_ora, ABSOLUTE_X, 3, 4, YESEC}},
        {0x19, {&Emu6502::op_ora, ABSOLUTE_Y, 3, 4, YESEC}},
        {0x01, {&Emu6502::op_ora, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0x11, {&Emu6502::op_ora, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // EOR (Logical Exclusive OR)
        {0x49, {&Emu6502::op_eor, IMMEDIATE, 2, 2, NOEC}},
        {0x45, {&Emu6502::op_eor, ZEROPAGE, 2, 3, NOEC}},
        {0x55, {&Emu6502::op_eor, ZEROPAGE_X, 2, 4, NOEC}},
        {0x4d, {&Emu6502::op_eor, ABSOLUTE, 3, 4, NOEC}},
        {0x5d, {&Emu6502::op_eor, ABSOLUTE_X, 3, 4, YESEC}},
        {0x59, {&Emu6502::op_eor, ABSOLUTE_Y, 3, 4, YESEC}},
        {0x41, {&Emu6502::op_eor, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0x51, {&Emu6502::op_eor, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // CLEAR STATUS
        {0x18, {&Emu6502::op_clc, IMPLICIT, 1, 2, NOEC}}, // CLC
        {0xd8, {&Emu6502::op_cld, IMPLICIT, 1, 2, NOEC}}, // CLD
        {0x58, {&Emu6502::op_cli, IMPLICIT, 1, 2, NOEC}}, // CLI
        {0xb8, {&Emu6502::op_clv, IMPLICIT, 1, 2, NOEC}}, // CLV
    
        // SET STATUS
        {0x38, {&Emu6502::op_sec, IMPLICIT, 1, 2, NOEC}}, // SEC
        {0xf8, {&Emu6502::op_sed, IMPLICIT, 1, 2, NOEC}}, // SED
        {0x78, {&Emu6502::op_sei, IMPLICIT, 1, 2, NOEC}}, // SEI
    
        // BIT SHIFT
        // LSR
        {0x4a, {&Emu6502::op_lsr_acc, ACCUMULATOR, 1, 2, NOEC}},
        {0x46, {&Emu6502::op_lsr_mem, ZEROPAGE, 2, 5, NOEC}},
        {0x56, {&Emu6502::op_lsr_mem, ZEROPAGE_X, 2, 6, NOEC}},
        {0x4e, {&Emu6502::op_lsr_mem, ABSOLUTE, 3, 6, NOEC}},
        {0x5e, {&Emu6502::op_lsr_mem, ABSOLUTE_X, 3, 7, NOEC}},
    
        // ASL
        {0x0a, {&Emu6502::op_asl_acc, ACCUMULATOR, 1, 2, NOEC}},
        {0x06, {&Emu6502::op_asl_mem, ZEROPAGE, 2, 5, NOEC}},
        {0x16, {&Emu6502::op_asl_mem, ZEROPAGE_X, 2, 6, NOEC}},
        {0x0e, {&Emu6502::op_asl_mem, ABSOLUTE, 3, 6, NOEC}},
        {0x1e, {&Emu6502::op_asl_mem, ABSOLUTE_X, 3, 7, NOEC}},
    
        // ROL
        {0x2a, {&Emu6502::op_rol_acc, ACCUMULATOR, 1, 2, NOEC}},
        {0x26, {&Emu6502::op_rol_mem, ZEROPAGE, 2, 5, NOEC}},
        {0x36, {&Emu6502::op_rol_mem, ZEROPAGE_X, 2, 6, NOEC}},
        {0x2e, {&Emu6502::op_rol_mem, ABSOLUTE, 3, 6, NOEC}},
        {0x3e, {&Emu6502::op_rol_mem, ABSOLUTE_X, 3, 7, NOEC}},
    
        // ROR
        {0x6a, {&Emu6502::op_ror_acc, ACCUMULATOR, 1, 2, NOEC}},
        {0x66, {&Emu6502::op_ror_mem, ZEROPAGE, 2, 5, NOEC}},
        {0x76, {&Emu6502::op_ror_mem, ZEROPAGE_X, 2, 6, NOEC}},
        {0x6e, {&Emu6502::op_ror_mem, ABSOLUTE, 3, 6, NOEC}},
        {0x7e, {&Emu6502::op_ror_mem, ABSOLUTE_X, 3, 7, NOEC}},
    
        // LOADS
        // LDA
        {0xa9, {&Emu6502::op_lda, IMMEDIATE, 2, 2, NOEC}},
        {0xa5, {&Emu6502::op_lda, ZEROPAGE, 2, 3, NOEC}},
        {0xb5, {&Emu6502::op_lda, ZEROPAGE_X, 2, 4, NOEC}},
        {0xad, {&Emu6502::op_lda, ABSOLUTE, 3, 4, NOEC}},
        {0xbd, {&Emu6502::op_lda, ABSOLUTE_X, 3, 4, YESEC}},
        {0xb9, {&Emu6502::op_lda, ABSOLUTE_Y, 3, 4, YESEC}},
        {0xa1, {&Emu6502::op_lda, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0xb1, {&Emu6502::op_lda, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        // LDX
        {0xa2, {&Emu6502::op_ldx, IMMEDIATE, 2, 2, NOEC}},
        {0xa6, {&Emu6502::op_ldx, ZEROPAGE, 2, 3, NOEC}},
        {0xb6, {&Emu6502::op_ldx, ZEROPAGE_Y, 2, 4, NOEC}},
        {0xae, {&Emu6502::op_ldx, ABSOLUTE, 3, 4, NOEC}},
        {0xbe, {&Emu6502::op_ldx, ABSOLUTE_Y, 3, 4, YESEC}},
    
        // LDY
        {0xa0, {&Emu6502::op_ldy, IMMEDIATE, 2, 2, NOEC}},
        {0xa4, {&Emu6502::op_ldy, ZEROPAGE, 2, 3, NOEC}},
        {0xb4, {&Emu6502::op_ldy, ZEROPAGE_X, 2, 4, NOEC}},
        {0xac, {&Emu6502::op_ldy, ABSOLUTE, 3, 4, NOEC}},
        {0xbc, {&Emu6502::op_ldy, ABSOLUTE_X, 3, 4, YESEC}},
    
        // STORE
        // STA
        {0x85, {&Emu6502::op_sta, ZEROPAGE, 2, 3, NOEC}},
        {0x95, {&Emu6502::op_sta, ZEROPAGE_X, 2, 4, NOEC}},
        {0x8d, {&Emu6502::op_sta, ABSOLUTE, 3, 4, NOEC}},
        {0x9d, {&Emu6502::op_sta, ABSOLUTE_X, 3, 5, NOEC}},
        {0x99, {&Emu6502::op_sta, ABSOLUTE_Y, 3, 5, NOEC}},
        {0x81, {&Emu6502::op_sta, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0x91, {&Emu6502::op_sta, POST_INDEX_INDIRECT, 2, 6, NOEC}},
    
        // STX
        {0x86, {&Emu6502::op_stx, ZEROPAGE, 2, 3, NOEC}},
        {0x96, {&Emu6502::op_stx, ZEROPAGE_Y, 2, 4, NOEC}},
        {0x8e, {&Emu6502::op_stx, ABSOLUTE, 3, 4, NOEC}},
    
        // STY
        {0x84, {&Emu6502::op_sty, ZEROPAGE, 2, 3, NOEC}},
        {0x94, {&Emu6502::op_sty, ZEROPAGE_X, 2, 4, NOEC}},
        {0x8c, {&Emu6502::op_sty, ABSOLUTE, 3, 4, NOEC}},
    
        // TRANSFER
        {0xaa, {&Emu6502::op_tax, IMPLICIT, 1, 2, NOEC}}, // TAX
        {0xa8, {&Emu6502::op_tay, IMPLICIT, 1, 2, NOEC}}, // TAY
        {0xba, {&Emu6502::op_tsx, IMPLICIT, 1, 2, NOEC}}, // TSX
        {0x8a, {&Emu6502::op_txa, IMPLICIT, 1, 2, NOEC}}, // TXA
        {0x9a, {&Emu6502::op_txs, IMPLICIT, 1, 2, NOEC}}, // TXS
        {0x98, {&Emu6502::op_tya, IMPLICIT, 1, 2, NOEC}}, // TYA
    
        // COMPARE
        {0xc9, {&Emu6502::op_cpa, IMMEDIATE, 2, 2, NOEC}},
        {0xc5, {&Emu6502::op_cpa, ZEROPAGE, 2, 3, NOEC}},
        {0xd5, {&Emu6502::op_cpa, ZEROPAGE_X, 2, 4, NOEC}},
        {0xcd, {&Emu6502::op_cpa, ABSOLUTE, 3, 4, NOEC}},
        {0xdd, {&Emu6502::op_cpa, ABSOLUTE_X, 3, 4, YESEC}},
        {0xd9, {&Emu6502::op_cpa, ABSOLUTE_Y, 3, 4, YESEC}},
        {0xc1, {&Emu6502::op_cpa, PRE_INDEX_INDIRECT, 2, 6, NOEC}},
        {0xd1, {&Emu6502::op_cpa, POST_INDEX_INDIRECT, 2, 5, YESEC}},
    
        {0xe0, {&Emu6502::op_cpx, IMMEDIATE, 2, 2, NOEC}},
        {0xe4, {&Emu6502::op_cpx, ZEROPAGE, 2, 3, NOEC}},
        {0xec, {&Emu6502::op_cpx, ABSOLUTE, 3, 4, NOEC}},
    
        {0xc0, {&Emu6502::op_cpy, IMMEDIATE, 2, 2, NOEC}},
        {0xc4, {&Emu6502::op_cpy, ZEROPAGE, 2, 3, NOEC}},
        {0xcc, {&Emu6502::op_cpy, ABSOLUTE, 3, 4, NOEC}},
    
        // STACK PUSH/PULL
        {0x48, {&Emu6502::op_pha, IMPLICIT, 1, 3, NOEC}}, // PHA
        {0x68, {&Emu6502::op_pla, IMPLICIT, 1, 4, NOEC}}, // PLA
        {0x08, {&Emu6502::op_php, IMPLICIT, 1, 3, NOEC}}, // PHP
        {0x28, {&Emu6502::op_plp, IMPLICIT, 1, 4, NOEC}}, // PLP
    
        // INCREASE / DECREASE
        {0xca, {&Emu6502::op_dex, IMPLICIT, 1, 2, NOEC}}, // DEX
        {0x88, {&Emu6502::op_dey, IMPLICIT, 1, 2, NOEC}}, // DEY
    
        {0xe8, {&Emu6502::op_inx, IMPLICIT, 1, 2, NOEC}}, // INX
        {0xc8, {&Emu6502::op_iny, IMPLICIT, 1, 2, NOEC}}, // INY
    
        {0xc6, {&Emu6502::op_dec, ZEROPAGE, 2, 5, NOEC}}, // DEC
        {0xd6, {&Emu6502::op_dec, ZEROPAGE_X, 2, 6, NOEC}}, // DEC
        {0xce, {&Emu6502::op_dec, ABSOLUTE, 3, 6, NOEC}}, // DEC
        {0xde, {&Emu6502::op_dec, ABSOLUTE_X, 3, 7, NOEC}}, // DEC
    
        {0xe6, {&Emu6502::op_inc, ZEROPAGE, 2, 5, NOEC}}, // INC
        {0xf6, {&Emu6502::op_inc, ZEROPAGE_X, 2, 6, NOEC}}, // INC
        {0xee, {&Emu6502::op_inc, ABSOLUTE, 3, 6, NOEC}}, // INC
        {0xfe, {&Emu6502::op_inc, ABSOLUTE_X, 3, 7, NOEC}}, // INC
    
        // BRANCH
        {0xd0, {&Emu6502::op_bne, IMPLICIT, 2, 2, BRANCHEC}}, // BNE
        {0xf0, {&Emu6502::op_beq, IMPLICIT, 2, 2, BRANCHEC}}, // BEQ
        {0x90, {&Emu6502::op_bcc, IMPLICIT, 2, 2, BRANCHEC}}, // BCC
        {0xb0, {&Emu6502::op_bcs, IMPLICIT, 2, 2, BRANCHEC}}, // BCS
        {0x30, {&Emu6502::op_bmi, IMPLICIT, 2, 2, BRANCHEC}}, // BMI
        {0x10, {&Emu6502::op_bpl, IMPLICIT, 2, 2, BRANCHEC}}, // BPL
        {0x50, {&Emu6502::op_bvc, IMPLICIT, 2, 2, BRANCHEC}}, // BVC
        {0x70, {&Emu6502::op_bvs, IMPLICIT, 2, 2, BRANCHEC}}, // BVS
    
        // JUMP
        {0x4c, {&Emu6502::op_jmp, ABSOLUTE, 0, 3, NOEC}}, // JMP
        {0x6c, {&Emu6502::op_jmp, INDIRECT, 0, 5, NOEC}}, // JMP
        {0x20, {&Emu6502::op_jsr, ABSOLUTE, 0, 6, NOEC}}, // JSR
        {0x60, {&Emu6502::op_rts, IMPLICIT, 0, 6, NOEC}}, // RTS
    };
};
