import numpy as np
import sounddevice as sd
import time
import threading

# https://python-sounddevice.readthedocs.io/en/0.5.1/examples.html#play-a-sine-signal

class AudioPlayer(threading.Thread):
    def __init__(self):
        super().__init__()
        self.frequency = 440 #707
        self.fs = 44100
        self.amplitude = 0.1
        self.samplerate = sd.query_devices(None, 'output')['default_samplerate']

        self.done = False
        self.start_idx = 0
        

    def callback(self, outdata, frames, time_, status):
        if status:
            print(status)
        t = (self.start_idx + np.arange(frames)) / self.samplerate
        t0 = t[0]
        t = t.reshape(-1, 1)
        # print(frames)

        prevfreq = self.frequency
        self.frequency = int(100 * np.sin(2*np.pi*self.start_idx/44000) + 600)
        outdata[:] = self.amplitude * np.sin(2 * np.pi * self.frequency * (t-t0) + 2 * np.pi * prevfreq * t0)
        self.start_idx += frames

    def run(self):
        self.done = False
        self.start_idx = 0

        with sd.OutputStream(device=None, channels=1, callback=self.callback,
                            samplerate=self.samplerate):
            while not self.done:
                time.sleep(0.1)


a = AudioPlayer()

a.run()
# a.start()
# try:
#     while True:
#         input()
#         a.frequency = int(a.frequency * 1.1)
#         print(a.frequency)
# except KeyboardInterrupt:
#     pass
# a.done = True
