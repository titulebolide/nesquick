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
    void validateChannelNo(int channel);
    float getWave(float phase, int duty_cycle);

public:
    SoundEngine();
    ~SoundEngine();
    void startSound();
    void setFrequency(int channel, float frequency, float duration);
    void setAmplitude(int channel, float amplitude);
    void setDutyCycle(int channel, float duty_cycle);
    void setChannelEnable(int channel, float enable);
    void generateSamples(Sint16 *stream, int length);
};
