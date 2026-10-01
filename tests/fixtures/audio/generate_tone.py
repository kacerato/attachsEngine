"""Original acceptance tone; Python standard library only, PCM16 stereo 48 kHz."""
import math, struct, wave
from pathlib import Path
rate, seconds = 48000, 6
with wave.open(str(Path(__file__).with_name("astra-audio-probe.wav")), "wb") as output:
    output.setparams((2, 2, rate, rate * seconds, "NONE", "not compressed"))
    frames = bytearray()
    for index in range(rate * seconds):
        t = index / rate
        envelope = min(1, t / .04, (seconds - t) / .04)
        signal = .13 * envelope * (.8 + .2 * math.sin(2 * math.pi * 2 * t))
        signal *= .75 * math.sin(2 * math.pi * 440 * t) + .25 * math.sin(2 * math.pi * 660 * t)
        value = round(signal * 32767)
        frames.extend(struct.pack("<hh", value, value))
    output.writeframes(frames)
