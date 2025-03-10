#include "apu.hpp"
#include "utils.hpp"

ApuDevice::ApuDevice() {
    return;
}

void ApuDevice::start_sound() {
    m_sound_engine.start_sound();
}

void ApuDevice::set_cpu(Emu6502 * cpu) {
    m_cpu = cpu;
}

uint8_t ApuDevice::get(uint16_t addr) {
    return 0;
}

void ApuDevice::set(uint16_t addr , uint8_t value) {
    uint8_t retval;

    switch (addr) {
    case KEY_PULSE1_DUTY_ENVELOPE:
        m_square1.volume = value & 0b1111;
        m_square1.constant_volume = ((value & BIT4) != 0);
        break;

    case KEY_PULSE1_PERIOD_LOW:
        m_square1.period = (m_square1.period & 0xff00) | static_cast<uint16_t>(value);
        break;
    
    case KEY_PULSE1_PERIOD_HIGH:
        m_square1.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_square1.period & 0x00ff);
        m_square1.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];

        std::cout << "sq1: " << m_square1.period << " " << static_cast<int>(m_square1.length) << std::endl;
        m_sound_engine.setFrequency(1, 1789773 /  (16.0f*( static_cast<float>(m_square1.period) + 1)), static_cast<float>(m_square1.length)/240.0f);
        break;

    case KEY_PULSE2_DUTY_ENVELOPE:
        break;
    
    case KEY_PULSE2_PERIOD_LOW:
        m_square2.period = (m_square2.period & 0xff00) | static_cast<uint16_t>(value);
        break;
    
    case KEY_PULSE2_PERIOD_HIGH:
        m_square2.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_square2.period & 0x00ff);
        m_square2.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];

        std::cout << "sq2: " << m_square2.period << " " << static_cast<int>(m_square2.length) << std::endl;
    case KEY_STATUS:
        // TODO : send 0 on powerup / reset
        // TODO : partially implemented
        m_square1.enable = ((value & BIT0) != 0);
        m_square2.enable = ((value & BIT1) != 0);
        break;

    case KEY_SETMODE:
        m_enable_irq = ((value & BIT6) == 0); // BIT6 is IRQ inhibit
        m_sequencer_mode = ((value & BIT6) != 0); // 0 : 4 step, 1 : 5 steps
        quarter_frame_tick();
        half_frame_tick();
        m_apu_cycle_count == 0;

    default:
        break;
    }

    return ;
}

// https://www.nesdev.org/wiki/APU_Frame_Counter
void ApuDevice::tick() {
    m_apu_cycle_count++;
    if (m_apu_cycle_count % APU_FRAME_CYCLE_COUNT == 0) {
        if (m_apu_cycle_count = APU_FRAME_CYCLE_COUNT) {
            // step 1
            quarter_frame_tick();
        } else if (m_apu_cycle_count = 2*APU_FRAME_CYCLE_COUNT) {
            // step 2
            quarter_frame_tick();
            half_frame_tick();

        } else if (m_apu_cycle_count = 3*APU_FRAME_CYCLE_COUNT) {
            // step 3
            // We run this step at cycle count 11184 but apparently
            // it runs normally at 11185
            // it probably don't matter
            quarter_frame_tick();

        } else if (m_apu_cycle_count = APU_FRAME_CYCLE_COUNT) {
            // step 4
            // We run this step at cycle count 14912 but apparently
            // it runs normally at 14914
            // it probably don't matter
            if (m_sequencer_mode == SEQUENCER_5STEP_MODE) {
                // in 5 step mode this step is skipped
                return;
            }
            quarter_frame_tick();
            half_frame_tick();
            m_apu_cycle_count == 0;

        } else if (m_apu_cycle_count = APU_FRAME_CYCLE_COUNT) {
            // step 5
            // runs at the right cycle count for this one
            // necessarily in 5 steps mode here because it has not been
            // reset on step 4
            quarter_frame_tick();
            half_frame_tick();
            m_apu_cycle_count == 0;

        }
    }
    return;
}

void ApuDevice::quarter_frame_tick() {
    // handle envelope
    if (m_square1.constant_volume) {
        std::cout << m_square1.volume << std::endl;
        m_sound_engine.setAmplitude(1, static_cast<float>(m_square1.volume)/15*14000);
    }
    if (m_square2.constant_volume) {
        m_sound_engine.setAmplitude(2, static_cast<float>(m_square2.volume)/15*14000);
    }
}

void ApuDevice::half_frame_tick() {
    
}
