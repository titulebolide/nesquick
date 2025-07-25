#include "apu.hpp"
#include "device.hpp"
#include <cstdint>
#include <iostream>
#include <ostream>

static float period_to_frequency(uint16_t period) {
    return CLOCK_FREQUENCY / (16.0f*( static_cast<float>(period) + 1));
}

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

void ApuDevice::set_duty_envelope(int chan, uint16_t value) {
    m_square[chan].duty_cycle_no = value >> 6;
    m_sound_engine.set_duty_cycle(chan, DUTY_CYCLE_VALUES[m_square[chan].duty_cycle_no]);
    m_square[chan].constant_volume = ((value & BIT4) != 0);
    if (m_square[chan].constant_volume) {
        m_square[chan].volume = value & 0b1111;
        m_square[chan].envolope_decay_speed = 0;
    } else {
        m_square[chan].volume = 15;
        m_square[chan].envolope_decay_speed = value & 0b1111;
        m_square[chan].decay_counter = m_square[chan].envolope_decay_speed;
    }
    m_sound_engine.set_amplitude(chan, static_cast<float>(m_square[chan].volume)/15*MAX_AMPLITUDE);
}

void ApuDevice::set_sweep(int chan, uint16_t value) {
    m_square[chan].sweep_enable = ((value & BIT7) != 0);
    m_square[chan].sweep_negate = ((value & BIT3) != 0);
    m_square[chan].sweep_period = ((value & (BIT6|BIT5|BIT4)) >> 4);
    m_square[chan].sweep_counter = m_square[chan].sweep_period;
    m_square[chan].sweep_shift_count = (value & (BIT2|BIT1|BIT0));
    // std::cout << "ena " << m_square[0].sweep_enable << " neg " << m_square[0].sweep_negate << " per " << (int)m_square[0].sweep_period << " cnt " << (int)m_square[0].sweep_counter << " sft " << (int)m_square[0].sweep_shift_count << std::endl;
}

void ApuDevice::set_period_low(int chan, int16_t value) {
    m_square[chan].period = (m_square[chan].period & 0xff00) | static_cast<uint16_t>(value);
}

void ApuDevice::set_period_high(int chan, int16_t value) {
    m_square[chan].period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_square[chan].period & 0x00ff);
    m_square[chan].length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];
    m_sound_engine.set_frequency(chan, period_to_frequency(m_square[chan].period));
    m_sound_engine.set_duration(chan, m_square[0].length / 240.0f);
}

void ApuDevice::handle_sweep(int chan) {
    if (m_square[chan].sweep_enable) {
        if (m_square[chan].sweep_counter == 0) {
            m_square[chan].sweep_counter = m_square[chan].sweep_period+1;
            uint16_t change_amout = m_square[chan].period >> (m_square[chan].sweep_shift_count);
            if (m_square[chan].sweep_negate && m_square[chan].period > MIN_PERIOD) {
                m_square[chan].period -= change_amout; // +1 only for pulse 0...
                if (chan == 0) {
                    // https://www.nesdev.org/wiki/APU_Sweep#Calculating_the_target_period
                    m_square[chan].period -= 1;
                }
            } else if (!m_square[chan].sweep_negate && m_square[chan].period < MAX_PERIOD) {
                m_square[chan].period += change_amout;
            }
            // std::cout << "period " << m_square[chan].period << std::endl;
            m_sound_engine.set_frequency(0, period_to_frequency(m_square[chan].period));
        }
        m_square[chan].sweep_counter--;
    }
}

void ApuDevice::set(uint16_t addr, uint8_t value) {
    uint8_t retval;
    float dur, freq;
    
    switch (addr) {
    case KEY_PULSE1_DUTY_ENVELOPE:
        set_duty_envelope(0, value);
        break;
        
    case KEY_PULSE1_SWEEP:
        set_sweep(0, value);
        break;

    case KEY_PULSE1_PERIOD_LOW:
        set_period_low(0, value);
        break;
        
    case KEY_PULSE1_PERIOD_HIGH:
        set_period_high(0, value);
        break;
        
    case KEY_PULSE2_DUTY_ENVELOPE:
        set_duty_envelope(1, value);
        break;

    case KEY_PULSE2_SWEEP:
        set_sweep(1, value);
        break;
            
    case KEY_PULSE2_PERIOD_LOW:
        set_period_low(1, value);
        break;
        
    case KEY_PULSE2_PERIOD_HIGH:
        set_period_high(1, value);
        break;
        
    case KEY_TRI_PERIOD_LOW:
        m_triangle.period = (m_triangle.period & 0xff00) | static_cast<uint16_t>(value);
        break;
        
    case KEY_TRI_PERIOD_HIGH:
        m_triangle.period = (static_cast<uint16_t>(value & 0b111) << 8) | (m_triangle.period & 0x00ff);
        
        m_triangle.length = APU_LENGTH_COUNTER_LOAD[(value & 0b11111000) >> 3];
        freq = CLOCK_FREQUENCY / 2.0f / (16.0f*( static_cast<float>(m_triangle.period) + 1));
        dur = m_triangle.length / 6.0f / 240.0f; // why 6 ?
        dur = static_cast<float>(static_cast<int>(dur*freq)/freq); // round duration to a multiple of the period, to prevent popping when ending the sound
        m_sound_engine.set_frequency(2, freq); 
        m_sound_engine.set_duration(2, dur);
        break;
        
    case KEY_STATUS:
        // TODO : send 0 on powerup / reset
        // TODO : partially implemented
        m_sound_engine.set_channel_enable(0, (value & BIT0) != 0);
        m_sound_engine.set_channel_enable(1, (value & BIT1) != 0);
        m_sound_engine.set_channel_enable(2, (value & BIT2) != 0);
        break;
        
    case KEY_SETMODE:
        m_enable_irq = ((value & BIT6) == 0); // BIT6 is IRQ inhibit
        m_sequencer_mode = ((value & BIT6) != 0); // 0 : 4 step, 1 : 5 steps
        // writing here trigger the reset of the apu counter
        // and call the quarter and half frame tick
        quarter_frame_tick();
        half_frame_tick();
        m_apu_cycle_count = 0;
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
        if (m_apu_cycle_count == APU_FRAME_CYCLE_COUNT) {
            // step 1
            quarter_frame_tick();
        } else if (m_apu_cycle_count == 2*APU_FRAME_CYCLE_COUNT) {
            // step 2
            half_frame_tick();
            quarter_frame_tick();
            
        } else if (m_apu_cycle_count == 3*APU_FRAME_CYCLE_COUNT) {
            // step 3
            // We run this step at cycle count 11184 but apparently
            // it runs normally at 11185
            // it probably don't matter
            quarter_frame_tick();
            
        } else if (m_apu_cycle_count == 4*APU_FRAME_CYCLE_COUNT) {
            // step 4
            // We run this step at cycle count 14912 but apparently
            // it runs normally at 14914
            // it probably don't matter
            if (m_sequencer_mode == SEQUENCER_5STEP_MODE) {
                // in 5 step mode this step is skipped
                return;
            }
            half_frame_tick();
            quarter_frame_tick();
            m_apu_cycle_count = 0;
            
        } else if (m_apu_cycle_count == 5*APU_FRAME_CYCLE_COUNT) {
            // step 5
            // runs at the right cycle count for this one
            // necessarily in 5 steps mode here because it has not been
            // reset on step 4
            half_frame_tick();
            quarter_frame_tick();
            m_apu_cycle_count = 0;
        }
    }
    return;
}

void ApuDevice::quarter_frame_tick() {
    // handle envelope
    
    for (int chan_no=0; chan_no < 2; chan_no++) {
        squarePulse * square = &m_square[chan_no];
        
        if (chan_no == 0 && m_square[0].sweep_enable && (m_square[0].period <= MIN_PERIOD || m_square[0].period >= MAX_PERIOD)) {
            m_sound_engine.set_amplitude(chan_no, 0);
        }
        
        if (!square->constant_volume && square->volume > 0) {
            if (square->decay_counter > 0) {
                square->decay_counter--;
            }
            if (square->decay_counter == 0) {
                square->volume--; // testted in the upper if that it was non zero
                m_sound_engine.set_amplitude(chan_no, static_cast<float>(square->volume)/15*MAX_AMPLITUDE);
                square->decay_counter = square->envolope_decay_speed;
            }
        }
    }
}

void ApuDevice::half_frame_tick() {
    handle_sweep(0);
    handle_sweep(1);
}
