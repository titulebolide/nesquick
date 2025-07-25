#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <cmath>
#include <iostream>

const int AMPLITUDE = 28000;
const int SAMPLE_RATE = 44100;
const float SAMPLE_RATE_PERIOD = 1.0f / SAMPLE_RATE;
const int BASE_FREQUENCY = 440;  // Base frequency in Hz (A4 note)
const float MODULATION_FREQUENCY = 10;  // Frequency of the frequency modulation in Hz

enum {
    PULSE_DUTY_12 = 0,
    PULSE_DUTY_25 = 1,
    PULSE_DUTY_50 = 2,
    PULSE_DUTY_25_NEGATED = 3,
};

struct squareWave {
    float frequency = 400;
    float left_duration = 0;
    float amplitude = AMPLITUDE/4; // TODO : try setting at 0 the init amplitude
    double current_phase = 0; // Tracks the phase of the wave
    float duty_cycle = 0.5;
    bool enabled = true;
};

class SoundEngine
{
private:
    Uint32 start_time;  // Tracks the start time for modulation
    squareWave m_square[3];
    void validate_channel_no(int channel);
    float get_wave(float phase, int duty_cycle);

public:
    SoundEngine();
    ~SoundEngine();
    void startSound();
    void set_frequency(int channel, float frequency);
    void set_duration(int channel, float duration);
    void set_amplitude(int channel, float amplitude);
    void set_duty_cycle(int channel, float duty_cycle);
    void set_channel_enable(int channel, float enable);
    void generate_samples(Sint16 *stream, int length);
};
