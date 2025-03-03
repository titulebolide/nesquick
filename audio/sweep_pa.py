import numpy as np
import pyaudio
import time

# Parameters
sample_rate = 44100  # Sampling rate in Hz
duration = 10  # Duration of the playback in seconds
amplitude = 0.5  # Amplitude of the sine wave
base_frequency = 440  # Base frequency in Hz (A4 note)
modulation_frequency = 1  # Frequency of the frequency modulation in Hz
chunk_size = 1024  # Number of samples per chunk

# Function to generate a sine wave with a constant frequency
def generate_sine_wave_chunk(frequency, num_samples, last_phase=0):
    # Generate time array for this chunk
    time_array = np.linspace(0, num_samples / sample_rate, num_samples, endpoint=False)
    # Calculate the phase for this chunk
    phase = last_phase + 2 * np.pi * frequency * time_array
    return amplitude * np.sin(phase), phase[-1]  # Return the wave and the last phase

# Callback function for pyaudio
def audio_callback(in_data, frame_count, time_info, status):
    global last_phase, start_time
    if status:
        print(f"Stream status: {status}")
    # Current time in seconds
    t = time.time() - start_time
    # Calculate the instantaneous frequency for this chunk (constant within the chunk)
    instantaneous_freq = base_frequency + 100 * np.sin(2 * np.pi * modulation_frequency * t)
    # Generate the sine wave chunk with the constant frequency
    chunk, last_phase = generate_sine_wave_chunk(instantaneous_freq, frame_count, last_phase=last_phase)
    # Return the chunk as bytes and continue flag
    return (chunk.astype(np.float32).tobytes(), pyaudio.paContinue)

# Global variables for phase and start time
last_phase = 0
start_time = time.time()

# Initialize PyAudio
p = pyaudio.PyAudio()

# Open audio stream with callback
stream = p.open(format=pyaudio.paFloat32,
                channels=1,
                rate=sample_rate,
                output=True,
                frames_per_buffer=chunk_size,
                stream_callback=audio_callback)

# Start the stream
stream.start_stream()
print("Playing audio... Press Ctrl+C to stop.")

# Keep the stream alive for the specified duration
try:
    while stream.is_active():
        time.sleep(0.1)
except KeyboardInterrupt:
    print("Stopping playback.")

# Stop and close the stream
stream.stop_stream()
stream.close()

# Terminate PyAudio
p.terminate()