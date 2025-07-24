#pragma once
#include "device.hpp"
#include "audio.hpp"
#include "cpu.hpp"

enum {
    KEY_PULSE1_DUTY_ENVELOPE = 0x4000,
    KEY_TEST1 = 0x4001,
    KEY_TEST2 = 0x4005,
    KEY_PULSE1_PERIOD_LOW = 0x4002,
    KEY_PULSE1_PERIOD_HIGH = 0x4003,

    KEY_PULSE2_DUTY_ENVELOPE = 0x4004,
    KEY_PULSE2_PERIOD_LOW = 0x4006,
    KEY_PULSE2_PERIOD_HIGH = 0x4007,

    KEY_TRI_SETUP = 0x4008,
    KEY_TRI_PERIOD_LOW = 0x400A,
    KEY_TRI_PERIOD_HIGH = 0x400B,

    // PPU starts at 0x4014
    // From here it is forwarded manually from the PPU

    KEY_STATUS = 0x4015,
    KEY_SETMODE = 0x4017,
};

struct squarePulse {
    uint16_t period = 0;
    uint8_t duty_cycle_no = 0;
    float length = 0;
    bool constant_volume = false;
    uint8_t volume = 0; // volume to be used in constant volume mode
    uint8_t envolope_decay_speed = 0;
    uint8_t decay_counter = 0;
    bool enable = 0;
};

struct trianglePulse {
    uint16_t period = 0;
    float length = 0;
};

static int const CLOCK_FREQUENCY = 1789773;
static int const MAX_AMPLITUDE = 4000;
const long APU_FRAME_CYCLE_COUNT = 3728; // NTSC

const float DUTY_CYCLE_VALUES[4] = {0.125, 0.25, 0.5, 0.75};

enum {
    SEQUENCER_4STEP_MODE = 0,
    SEQUENCER_5STEP_MODE = 1,
};

const uint8_t APU_LENGTH_COUNTER_LOAD[32] = {10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

class ApuDevice : public Device {
 public:
    ApuDevice();
    void tick();
    uint8_t get(uint16_t addr);
    void set(uint16_t addr, uint8_t val);
    void start_sound();
    void set_cpu(Emu6502 * cpu);

 private:
    void quarter_frame_tick();
    void half_frame_tick();
    void set_duty_envelope(int chan, uint16_t value);

private:
    // TODO : needed to call IRQ, bu can do better than this
    Emu6502 * m_cpu;
    squarePulse m_square[2];
    trianglePulse m_triangle;

    // set by 0x4017
    // TODO : handle IRQ flag?
    bool m_enable_irq = false;
    bool m_sequencer_mode = false;

    long m_apu_cycle_count = 0;

    SoundEngine m_sound_engine;
};
