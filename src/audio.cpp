#include "audio.hpp"

void audio_callback(void*, Uint8*, int);

SoundEngine::SoundEngine() {}

void SoundEngine::startSound() {
    SDL_AudioSpec desiredSpec;

    desiredSpec.freq = SAMPLE_RATE;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 1;
    desiredSpec.samples = 256;
    desiredSpec.callback = audio_callback;
    desiredSpec.userdata = this;

    SDL_AudioSpec obtainedSpec;

    if (SDL_OpenAudio(&desiredSpec, &obtainedSpec) < 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Initialize phase and start time
    start_time = SDL_GetTicks();

    // Start playing audio
    SDL_PauseAudio(0);
}

SoundEngine::~SoundEngine()
{
    SDL_CloseAudio();
}

void SoundEngine::validateChannelNo(int channel) {
    if (!(channel <= 2 && channel >= 0)) {
        throw std::runtime_error("Bad channel number");
    }
}

void SoundEngine::setFrequency(int channel, float frequency, float duration)
{   
    validateChannelNo(channel);
    m_square[channel].frequency = frequency;
    m_square[channel].left_duration = duration;
}

void SoundEngine::setAmplitude(int channel, float amplitude)
{   
    validateChannelNo(channel);
    m_square[channel].amplitude = amplitude;
}

void SoundEngine::setDutyCycle(int channel, float duty_cycle)
{   
    validateChannelNo(channel);
    m_square[channel].duty_cycle = duty_cycle;
}

void SoundEngine::setChannelEnable(int channel, float enable)
{   
    validateChannelNo(channel);
    m_square[channel].enabled = enable;
}

void SoundEngine::generateSamples(Sint16 *stream, int length)
{
    for (int i = 0; i < length; i++) {
        stream[i] = 0;
        
        for (int chan_no=0; chan_no < 2; chan_no++) {
            squareWave * channel = &m_square[chan_no];
            // std::cout << channel->enabled << " " << channel->left_duration << " " << channel->duty_cycle << " " << channel->current_phase << " " << channel->amplitude << std::endl;
            if (!(channel->enabled && channel->left_duration > 0)) {
                continue;
            }
            stream[i] += channel->amplitude * ((channel->current_phase < 2 * M_PI * channel->duty_cycle) ? 1.0f:-1.0f);
            channel->left_duration -= 1.0f/SAMPLE_RATE;
            // increase phase only if playing
            channel->current_phase += 2 * M_PI * channel->frequency / SAMPLE_RATE;
            // Wrap phase to avoid overflow
            if (channel->current_phase > 2 * M_PI) {
                channel->current_phase -= 2 * M_PI;
            }
        }

        squareWave * channel = &m_square[2];

        if ((channel->enabled && channel->left_duration > 0)) {
            if (channel->current_phase < M_PI) {
                stream[i] += channel->amplitude * (channel->current_phase/M_PI*2 - 1);
            } else {
                stream[i] += channel->amplitude * (3 - channel->current_phase/M_PI*2);
            }
        }
        channel->left_duration -= 1.0f/SAMPLE_RATE;
        // increase phase only if playing
        channel->current_phase += 2 * M_PI * channel->frequency / SAMPLE_RATE;
        // Wrap phase to avoid overflow
        if (channel->current_phase > 2 * M_PI) {
            channel->current_phase -= 2 * M_PI;
        }

    }
}

void audio_callback(void *_beeper, Uint8 *_stream, int _length)
{
    Sint16 *stream = (Sint16*) _stream;
    int length = _length / 2;
    SoundEngine* beeper = (SoundEngine*) _beeper;

    beeper->generateSamples(stream, length);
}
