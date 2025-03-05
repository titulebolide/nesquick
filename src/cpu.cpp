#include <iostream>
#include <vector>
#include <map>
#include <stdexcept>
#include <iomanip>
#include <bitset>
#include <thread>
#include <chrono>

#include "utils.hpp"
#include "cpu.hpp"

Emu6502::Emu6502(Memory *mem, bool debug, LstDebuggerAsm6 *lst)
    : debug(debug), mem(mem), lst(lst) {
    regs = {0, 0, 0, 0};
    stack_ptr = 0xff;
    prgm_ctr = 0;
    interrupt_type = INTERRUPT_RST;
    instruction_cycle = 0;
    instruction_nbcycles = 0;

    check_opcode_map();
}


void Emu6502::check_opcode_map() {
    for (const auto& pair : opcodes) {
        if (pair.second.extra_cycle_type == YESEC) {
            if (pair.second.addr_mode != ABSOLUTE_X && pair.second.addr_mode != ABSOLUTE_Y && pair.second.addr_mode != POST_INDEX_INDIRECT) {
                throw std::runtime_error("Invalid opcode map");
            }
        }
    }
}

void Emu6502::set_status_bit(uint8_t status_bit, bool on) {
    if (on) {
        regs[REG_S] |= status_bit;
    } else {
        regs[REG_S] &= byte_not(status_bit);
    }
}

bool Emu6502::get_status_bit(uint8_t status_bit) {
    return (regs[REG_S] & status_bit) != 0;
}

uint16_t Emu6502::get_addr(int mode, bool * page_crossed) {
    *page_crossed = false;
    bool dummy_bool;
    uint16_t addr = 0;

    if (mode == ABSOLUTE || mode == ABSOLUTE_X || mode == ABSOLUTE_Y) {
        addr = mem->get(prgm_ctr + 1) + (mem->get(prgm_ctr + 2) << 8);
        uint8_t base_page_no = high_byte(addr);
        if (mode == ABSOLUTE_X) {
            addr += regs[REG_X];
            addr &= 0xffff;
        } else if (mode == ABSOLUTE_Y) {
            addr += regs[REG_Y];
            addr &= 0xffff;
        }
        uint8_t new_page_no = high_byte(addr);
        *page_crossed = (new_page_no != base_page_no);

    } else if (mode == ZEROPAGE || mode == ZEROPAGE_X || mode == ZEROPAGE_Y) {
        addr = mem->get(prgm_ctr + 1);
        if (mode == ZEROPAGE_X) {
            addr += regs[REG_X];
            addr &= 0xff;
        } else if (mode == ZEROPAGE_Y) {
            addr += regs[REG_Y];
            addr &= 0xff;
        }

    } else if (mode == INDIRECT) {
        uint16_t implicit_addr = get_addr(ABSOLUTE, &dummy_bool);
        uint8_t dest_addr_lsb = mem->get(implicit_addr);
        uint8_t dest_addr_msb = mem->get((implicit_addr + 1) & 0xffff);
        addr = dest_addr_lsb + (dest_addr_msb << 8);

    } else if (mode == PRE_INDEX_INDIRECT) {
        uint16_t implicit_addr = get_addr(ZEROPAGE_X, &dummy_bool);
        uint8_t dest_addr_lsb = mem->get(implicit_addr);
        uint8_t dest_addr_msb = mem->get((implicit_addr + 1) & 0xffff);
        addr = dest_addr_lsb + (dest_addr_msb << 8);

    } else if (mode == POST_INDEX_INDIRECT) {
        uint16_t implicit_addr = get_addr(ZEROPAGE, &dummy_bool);
        uint8_t dest_addr_lsb = mem->get(implicit_addr);
        uint8_t dest_addr_msb = mem->get((implicit_addr + 1) & 0xffff);
        addr = dest_addr_lsb + (dest_addr_msb << 8);
        uint8_t base_page_no = high_byte(addr);
        addr += regs[REG_Y];
        addr &= 0xffff;
        uint8_t new_page_no = high_byte(addr);
        *page_crossed = (new_page_no != base_page_no);

    } else if (mode == IMMEDIATE) {
        *page_crossed = false;
        addr = prgm_ctr + 1;
    } else {
        throw std::runtime_error("Invalid addressing mode");
    }

    return addr;
}


void Emu6502::update_zn_flag(uint8_t value) {
    set_status_bit(STATUS_ZERO, value == 0);
    set_status_bit(STATUS_NEG, (value & 0b10000000) != 0);
}

void Emu6502::op_jmp() {
    prgm_ctr = op_addr;
}

void Emu6502::stack_push(uint8_t val) {
    uint16_t stack_addr = stack_ptr + 0x0100;
    mem->set(stack_addr, val);
    stack_ptr--;
}

uint8_t Emu6502::stack_pull() {
    stack_ptr++;
    uint16_t stack_addr = stack_ptr + 0x0100;
    return mem->get(stack_addr);
}

/*
Deepseek fixed a bug from python ??
In python we performed only one push, but of an int16 (the full addr)
It was working but not mimicking the nes behaviour!
*/
void Emu6502::op_jsr() {
    stack_push(high_byte(prgm_ctr + 2));
    stack_push(low_byte(prgm_ctr + 2));
    op_jmp();
}


/*
Same here !
*/
void Emu6502::op_rts() {
    uint8_t low = stack_pull();
    uint8_t high = stack_pull();
    prgm_ctr = (high << 8) + low + 1;
}

void Emu6502::ph(int reg) {
    // stack begins at 0x01ff and ends at 0x0100
    if (stack_ptr == 0) {
        throw std::runtime_error("Stack overflow");
    }
    stack_push(regs[reg]);
}

void Emu6502::pl(int reg) {
    // stack begins at 0x01ff and ends at 0x0100
    if (stack_ptr == 0xff) {
        throw std::runtime_error("Empty stack");
    }
    uint8_t val = stack_pull();
    regs[reg] = val;
    if (reg != REG_S) {
        // for REG_S it is already handled!
        update_zn_flag(val);
    }
}

void Emu6502::op_bit() {
    // https://www.masswerk.at/6502/6502_instruction_set.html#bitcompare
    uint8_t acc = regs[REG_A];
    uint8_t val = mem->get(op_addr);
    set_status_bit(STATUS_ZERO, (acc & val) == 0);
    set_status_bit(STATUS_NEG, (val & 0b10000000) != 0);
    set_status_bit(STATUS_OVFLO, (val & 0b01000000) != 0);
}

void Emu6502::load(int reg, uint8_t val) {
    // load accumulator
    regs[reg] = val;
    update_zn_flag(val);
}

void Emu6502::store(int reg, uint16_t addr) {
    uint8_t val = regs[reg];
    mem->set(addr, val);
}

void Emu6502::transfer(int sreg, int dreg) {
    uint8_t val = regs[sreg];
    regs[dreg] = val;
    update_zn_flag(val);
}

void Emu6502::compare(int reg, uint8_t val) {
    bool is_carry = (static_cast<uint16_t>(regs[reg]) + static_cast<uint16_t>(byte_not(val)) + 1) > 255;
    int diff = regs[reg] - val;
    set_status_bit(STATUS_CARRY, is_carry);
    update_zn_flag(diff); // status_zero goes to 0 if equality
}

void Emu6502::in_de_reg(int reg, bool sign_plus) {
    if (sign_plus) {
        regs[reg]++;
    } else {
        regs[reg]--;
    }
    regs[reg] &= 0xff;
    update_zn_flag(regs[reg]);
}

void Emu6502::op_inx() {
    in_de_reg(REG_X, true);
}

void Emu6502::op_dex() {
    in_de_reg(REG_X, false);
}

void Emu6502::op_iny() {
    in_de_reg(REG_Y, true);
}

void Emu6502::op_dey() {
    in_de_reg(REG_Y, false);
}

void Emu6502::in_de_mem(uint16_t addr, bool sign_plus) {
    uint8_t val = mem->get(addr);
    if (sign_plus) {
        val++;
    } else {
        val--;
    }
    val &= 0xff;
    mem->set(addr, val);
    update_zn_flag(val);
}

void Emu6502::add_val_to_acc_carry(uint8_t val) {
    // use a uint16_t to detect for a carry
    // TODO : maybe remove some of the static cast ? 
    uint16_t bigval = static_cast<uint16_t>(val);
    if (get_status_bit(STATUS_CARRY)) {
        // there is a carry
        bigval++;
    }
    bigval += static_cast<uint16_t>(regs[REG_A]);
    // regs[REG_A] += val;
    set_status_bit(STATUS_CARRY, bigval > 255);
    regs[REG_A] = static_cast<uint8_t>(bigval);
    update_zn_flag(regs[REG_A]);
}

void Emu6502::op_adc() {
    add_val_to_acc_carry(mem->get(op_addr));
}

void Emu6502::op_sbc() {
    // use two's complement https://stackoverflow.com/a/41253661
    // TODO : check it should not be 
    // add_val_to_acc_carry(byte_not(mem[addr]) + 1)
    add_val_to_acc_carry(byte_not(mem->get(op_addr)));
}

void Emu6502::op_and() {
    regs[REG_A] &= mem->get(op_addr);
    update_zn_flag(regs[REG_A]);
}

void Emu6502::op_ora() {
    regs[REG_A] |= mem->get(op_addr);
    update_zn_flag(regs[REG_A]);
}

void Emu6502::op_eor() {
    regs[REG_A] ^= mem->get(op_addr);
    update_zn_flag(regs[REG_A]);
}

void Emu6502::branch(uint8_t status_bit, bool branch_if_zero) {
    int extra_cycles = 0;
    bool do_branch = false;
    if (get_status_bit(status_bit) == 0) {
        do_branch = true;
    }
    if (!branch_if_zero) {
        do_branch = !do_branch;
    }
    if (do_branch) {
        // cast to int8_t to takeaccount for a sign
        int8_t branch_addr = static_cast<int8_t>(mem->get(prgm_ctr + 1));
        uint8_t base_page = high_byte(prgm_ctr);
        prgm_ctr += branch_addr;
        // no need to crop to 65536 because it is a uint16_t
        uint8_t new_page = high_byte(prgm_ctr);
        if (base_page == new_page) {
            extra_cycles = 1;
        } else {
            extra_cycles = 2;
        }
    }
    // update current op extra cycles
    op_extra_cycles += extra_cycles;
}


uint8_t Emu6502::shift_right(uint8_t val) {
    bool carry_on = (val & 0b00000001) != 0;
    val >>= 1;
    update_zn_flag(val);
    set_status_bit(STATUS_CARRY, carry_on);
    return val;
}

uint8_t Emu6502::shift_left(uint8_t val) {
    bool carry_on = (val & 0b10000000) != 0;
    val <<= 1;
    // no need to truncadte (uint8_t)
    update_zn_flag(val);
    set_status_bit(STATUS_CARRY, carry_on);
    return val;
}

uint8_t Emu6502::rotate_right(uint8_t val) {
    bool curr_carry = get_status_bit(STATUS_CARRY);
    bool next_carry = (val & 0b00000001) != 0;
    val >>= 1;
    val |= (curr_carry << 7);
    update_zn_flag(val);
    set_status_bit(STATUS_CARRY, next_carry);
    return val;
}

uint8_t Emu6502::rotate_left(uint8_t val) {
    bool curr_carry = get_status_bit(STATUS_CARRY);
    bool next_carry = (val & 0b10000000) != 0;
    val <<= 1;
    // no need to crop (uint8_t)
    val |= curr_carry; // bool true is interpreted as a 1
    update_zn_flag(val);
    set_status_bit(STATUS_CARRY, next_carry);
    return val;
}

void Emu6502::hw_interrupt(bool maskable) {
    /*
    Maskable = true : IRQ
    Maskable = false : NMI

    don't do anything if IRQ and IRQ has been disabled
    */
    if (maskable && get_status_bit(STATUS_INTER)) {
        return;
    }
    stack_push(high_byte(prgm_ctr));
    stack_push(low_byte(prgm_ctr));
    stack_push(regs[REG_S]);
    uint16_t prgm_ctr_addr = maskable ? 0xfffe : 0xfffa;
    prgm_ctr = (mem->get(prgm_ctr_addr + 1) << 8) + mem->get(prgm_ctr_addr);
}

void Emu6502::op_nmi() {
    set_status_bit(STATUS_BREAK, false);
    hw_interrupt(false);
}

void Emu6502::op_irq() {
    set_status_bit(STATUS_BREAK, false);
    hw_interrupt(true);
}

void Emu6502::op_brk() {
    /*
    Break is like an NMI, but instead to store PC in stack we store PC+2
    So we increade PC by 2 and fwd to hw interrupt
    */
    set_status_bit(STATUS_BREAK, true);
    prgm_ctr += 2;
    hw_interrupt(false);
}

void Emu6502::op_rti() {
    uint8_t old_status = stack_pull();
    uint8_t curr_status = regs[REG_S];

    // we want to keep the same value for bit 4 (break) and 5
    uint8_t status_ignore_mask = STATUS_BREAK | STATUS_BIT5;
    uint8_t status_ignore_mask_bar = byte_not(status_ignore_mask);
    // set to 1 the ignored bits
    old_status |= status_ignore_mask;
    // set to 1 the unignored bits
    curr_status |= status_ignore_mask_bar;

    regs[REG_S] = old_status & curr_status; // = 0bxx11xxxx & 0b11yy1111 = 0bxxyyxxxx

    set_status_bit(STATUS_BREAK, false);
    set_status_bit(STATUS_BIT5, false);

    uint8_t pc_low = stack_pull();
    uint8_t pc_high = stack_pull();
    prgm_ctr = (pc_high << 8) + pc_low;
}

void Emu6502::dbg() {
    if (prgm_ctr == 0) {
        return;
    }
    std::string inst = "";
    if (lst != nullptr) {
        inst = lst->getInst(prgm_ctr);
    }
    std::cout << "\nPC\tinst\tA\tX\tY\tSP\tNV-BDIZC\n";
    std::cout << std::hex << prgm_ctr << "\t" << hex2(mem->get(prgm_ctr)) << "\t" << hex2(regs[REG_A]) << "\t" << hex2(regs[REG_X]) << "\t" << hex2(regs[REG_Y]) << "\t" << hex2(stack_ptr) << "\t" << bin8(regs[REG_S]) << "\n";
    std::cout << inst << std::endl;
    if (inst.find("bkpt") != std::string::npos) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void Emu6502::interrupt(bool maskable) {
    /*
    This will set the interrupt type, causing the trigger of an
    hw interrupt at the next op execution
    Made to be called externally
    */
    if (maskable) {
        interrupt_type = INTERRUPT_IRQ;
    } else {
        interrupt_type = INTERRUPT_NMI;
    }
}

void Emu6502::op_reset() {
    uint16_t reset_vector = 0xfffc;
    prgm_ctr = (mem->get(reset_vector + 1) << 8) + mem->get(reset_vector);
}

int Emu6502::exec_inst() {
    if (debug) {
        dbg();
    }

    uint16_t opcode = 0;
    if (interrupt_type != INTERRUPT_NO) {
        // hw interrupt is requested
        // retreive the fake opcode to run the instruct
        // as if it was any other function
        if (interrupt_type == INTERRUPT_IRQ) {
            opcode = OPCODE_IRQ;
        } else if (interrupt_type == INTERRUPT_NMI) {
            opcode = OPCODE_NMI;
        } else if (interrupt_type == INTERRUPT_RST) {
            opcode = OPCODE_RST;
        } else {
            throw std::runtime_error("Invalid interrupt type");
        }
        // reset interrupt type
        interrupt_type = INTERRUPT_NO;
    } else {
        // no interrupt, run the next intruction normally
        opcode = mem->get(prgm_ctr);
    }

    if (opcodes.find(opcode) == opcodes.end()) {
        throw std::runtime_error("Unknown opcode");
    }

    auto& op = opcodes[opcode];

    // holds the addr specified depending on the addressing scheme
    op_addr = 0;
    // op_extra_cycles used only by branch ot report if the branching caused an extrac cycle
    op_extra_cycles = 0;

    if (!(op.addr_mode == IMPLICIT || op.addr_mode == ACCUMULATOR)) {
        bool page_crossed;
        op_addr = get_addr(op.addr_mode, &page_crossed);
        if (page_crossed) {
            op_extra_cycles = 1;
        }
    }
    (this->*op.func)();

    uint ncycle = op.base_ncycle + op_extra_cycles;
    prgm_ctr += op.nbytes;
    return ncycle;
}

bool Emu6502::tick() {
    // run instruction if we are at the beggining of the cycle
    // else just register the tick
    if (instruction_cycle == 0) {
        instruction_nbcycles = exec_inst();
        if (instruction_nbcycles == -1) {
            return false;
        }
    }
    instruction_cycle++;
    if (instruction_cycle == instruction_nbcycles) {
        instruction_cycle = 0;
    }
    return true;
}
