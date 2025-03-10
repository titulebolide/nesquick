#include "apu.hpp"
#include "utils.hpp"

ApuDevice::ApuDevice() {
    return;
}

void ApuDevice::start_sound() {
    m_sound_engine.startSound();
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
        m_square1.duty_cycle_no = value >> 6;
        m_sound_engine.setDutyCycle(0, DUTY_CYCLE_VALUES[m_square1.duty_cycle_no]);
        m_square1.volume = value & 0b1111;
        m_square1.constant_volume = ((value & BIT4) != 0);
        break;

    case KEY_PULSE1_PERIOD_LOW:
        m_square1.period = (m_square1.period & 0xff00) | static_cast<uint16_t>(value);
        break;
    
    case KEY_PULSE1_PERIOD_HIGH:
        m_square1.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_square1.period & 0x00ff);
        m_square1.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];
        m_sound_engine.setFrequency(
            0, 
            1789773 /  (16.0f*( static_cast<float>(m_square1.period) + 1)), 
            m_square1.length / 240.0f
        );
        break;

    case KEY_PULSE2_DUTY_ENVELOPE:
        m_square1.duty_cycle_no = value >> 6;
        m_sound_engine.setDutyCycle(1, DUTY_CYCLE_VALUES[m_square1.duty_cycle_no]);
        m_square2.volume = value & 0b1111;
        m_square2.constant_volume = ((value & BIT4) != 0);
        break;
    
    case KEY_PULSE2_PERIOD_LOW:
        m_square2.period = (m_square2.period & 0xff00) | static_cast<uint16_t>(value);
        break;
    
    case KEY_PULSE2_PERIOD_HIGH:
        m_square2.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_square2.period & 0x00ff);
        m_square2.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];
        m_sound_engine.setFrequency(
            1, 
            1789773 /  (16.0f*( static_cast<float>(m_square2.period) + 1)), 
            m_square2.length / 240.0f
        );
        break;

    case KEY_TRI_PERIOD_LOW:
        m_triangle.period = (m_triangle.period & 0xff00) | static_cast<uint16_t>(value);
        break;

    case KEY_TRI_PERIOD_HIGH:
        m_triangle.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_triangle.period & 0x00ff);
        m_triangle.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];
        m_sound_engine.setFrequency(2, 1789773 / (16.0f*( static_cast<float>(m_square2.period) + 1)) / 2, m_square2.length / 240.0f);
        break;

    case KEY_STATUS:
        // TODO : send 0 on powerup / reset
        // TODO : partially implemented
        m_sound_engine.setChannelEnable(0, (value & BIT0) != 0);
        m_sound_engine.setChannelEnable(1, (value & BIT1) != 0);
        m_sound_engine.setChannelEnable(2, (value & BIT2) != 0);
        break;

    case KEY_SETMODE:
        m_enable_irq = ((value & BIT6) == 0); // BIT6 is IRQ inhibit
        m_sequencer_mode = ((value & BIT6) != 0); // 0 : 4 step, 1 : 5 steps
        // writing here trigger the reset of the apu counter
        // and call the quarter and half frame tick
        quarter_frame_tick();
        half_frame_tick();
        m_apu_cycle_count == 0;
        break;

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
    float amplitude1, amplitude2 = 0;
    if (m_square1.constant_volume) {
        amplitude1 = static_cast<float>(m_square1.volume)/15*14000;
    }
    if (m_square2.constant_volume) {
        amplitude2 = static_cast<float>(m_square2.volume)/15*14000;
    }
    // m_sound_engine.setAmplitude(0, amplitude1);
    // m_sound_engine.setAmplitude(1, amplitude2);

}

void ApuDevice::half_frame_tick() {
    
}
