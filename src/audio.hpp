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

class SoundEngine
{
private:
    double phase1;  // Tracks the phase of the sine wave
    double phase2;  // Tracks the phase of the sine wave
    Uint32 start_time;  // Tracks the start time for modulation
    float m_frequency_1;
    float m_frequency_2;
    float m_sq1_cur_dur;
    float m_sq2_cur_dur;
    float m_sq1_amplitude;
    float m_sq2_amplitude;

public:
    SoundEngine();
    ~SoundEngine();
    void start_sound();
    void setFrequency(int channel, float frequency, float duration);
    void setAmplitude(int channel, float amplitude);
    void generateSamples(Sint16 *stream, int length);
};
