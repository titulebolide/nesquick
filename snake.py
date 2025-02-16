import numpy as np
import matplotlib.pyplot as plt

import matplotlib.animation as animation

from emu6502 import Emu6502

import threading

import random

plt.rcParams['keymap.save'].remove('s')
plt.rcParams['keymap.quit'].remove('q')


class AnimatedImshow:
    def __init__(self, data_provider, interval=50):
        """
        Initialize the AnimatedImshow class.

        Parameters:
        - data_provider: An instance of DataProvider.
        - interval: The delay between frames in milliseconds.
        - cmap: The colormap to use for the imshow.
        """
        self.data_provider = data_provider
        self.interval = interval
        self.fig, self.ax = plt.subplots()
        self.im = self.ax.imshow(np.random.rand(32,32)) # if i don't use a random init vector it don't work???

    def update(self, frame):
        """
        Update the image for the given frame.

        Parameters:
        - frame: The current frame index.
        """
        mat = np.array(self.data_provider.mem[0x0200:0x0600]).reshape((32,32))
        self.im.set_array(mat)
        return [self.im]

    def animate(self):
        """
        Create and display the animation.
        """
        self.ani = animation.FuncAnimation(
            self.fig, self.update, interval=self.interval, blit=True)
        plt.show()


# l = LstDebugger()

with open("rom/snake/test.bin", "rb") as f:
    rom = f.read()

class RandomGen:
    def __getitem__(self, value):
        return int(random.random()*256)

import evdev
class KbDevice(threading.Thread):
    def __init__(self):
        super().__init__()
        self.asciival = 0

    def run(self):
        dev = evdev.InputDevice('/dev/input/event4')
        for event in dev.read_loop():
            if event.type != evdev.ecodes.EV_KEY:
                continue
            key = evdev.ecodes.KEY[event.code][4:]
            if len(key) != 1:
                continue
            if event.value != 1: #press down
                continue
            self.asciival = ord(key.lower())

    def __getitem__(self, value):
        return self.asciival

kb = KbDevice()

mmap = [
    (0x0000, [0]*(1<<15)),
    (0xfe, RandomGen()),
    (0xff, kb),
    (0x100, [0]*(1<<15)),
    (0x8000, rom),
]

emu = Emu6502(mmap)

m = AnimatedImshow(emu)

kb.start()
emu.start()
m.animate()
kb.join()
