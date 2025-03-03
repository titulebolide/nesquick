import time

import numpy as np
import pyaudio

# p = pyaudio.PyAudio()

# volume = 0.5  # range [0.0, 1.0]
# fs = 44100  # sampling rate, Hz, must be integer
# duration = 2.0  # in seconds, may be float
# f = 440.0  # sine frequency, Hz, may be float

# # generate samples, note conversion to float32 array
# samples = (np.sin(2 * np.pi * np.arange(fs * duration) * f / fs)).astype(np.float32)

# # per @yahweh comment explicitly convert to bytes sequence
# output_bytes = (volume * samples).tobytes()

# print("aaaaa")
# # for paFloat32 sample values must be in range [-1.0, 1.0]
# stream = p.open(format=pyaudio.paFloat32,
#                 channels=1,
#                 rate=fs,
#                 output=True)

# # play. May repeat with different volume values (if done interactively)
# start_time = time.time()
# stream.write(output_bytes)
# print("Played sound for {:.2f} seconds".format(time.time() - start_time))

# stream.stop_stream()
# stream.close()

# p.terminate()
import threading
import time

class AudioPlayer(threading.Thread):
    def __init__(self):
        super().__init__()
        self.freq = 707
        self.fs = 44100
        self.done = False

    def callback(self):
        # lets make 5 periods
        # print(self.freq)
        duration = 0.01 # s (target duration, will be cropped to a multiple of the period)
        period = 1/self.freq
        duration = period * (duration//period) - 1/self.fs # - 150/self.fs
        # print(duration)
        return np.sin(
            2 * np.pi * np.arange(self.fs * duration) * self.freq / self.fs
        ).astype(np.float32) #, np.arange(self.fs * duration)

# samples = (np.sin(2 * np.pi * np.arange(fs * duration) * f / fs)).astype(np.float32)


    def run(self):
        self.done = False
        # Instantiate PyAudio and initialize PortAudio system resources (1)
        p = pyaudio.PyAudio()

        # Open stream (2)
        stream = p.open(format=pyaudio.paFloat32,
                        channels=1,
                        rate=self.fs,
                        frames_per_buffer=4410, # 100ms
                        output=True)

        while not self.done:
            data = self.callback()
            len_dat = len(data)
            while stream.get_write_available() < len_dat:
                time.sleep(0.001)

                # print(i, len_dat)
            stream.write(data)

        # Close stream (4)
        stream.close()

        # Release PortAudio system resources (5)
        p.terminate()


a = AudioPlayer()

a.start()
try:
    while True:
        input()
        a.freq = int(a.freq * 1.1)
        print(a.freq)
except KeyboardInterrupt:
    pass
a.done = True

# import matplotlib.pyplot as plt
# s, t  = a.callback()

# plt.plot(s.tolist() + s.tolist())
# plt.show()