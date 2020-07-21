# Bridge between pulseaudio and python

import ctypes
from math import sin, tau


__all__ = ["init", "send_string"]

## Pulse
pa_lib = ctypes.cdll.LoadLibrary('libpulse-simple.so.0')

_pa_simple_new = pa_lib.pa_simple_new
_pa_simple_new.restype = ctypes.c_voidp

_pa_simple_write = pa_lib.pa_simple_write
_pa_simple_write.argtypes = [ctypes.c_voidp, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t, ctypes.POINTER(ctypes.c_int)]

_pa_simple_drain = pa_lib.pa_simple_drain
_pa_simple_drain.argtypes = [ctypes.c_voidp, ctypes.POINTER(ctypes.c_int)]


PA_STREAM_PLAYBACK = 1
PA_SAMPLE_S16LE = 3
PA_SAMPLE_FLOAT32LE = 5


class struct_pa_sample_spec(ctypes.Structure):
    __slots__ = [
      'format',
      'rate',
      'channels',
    ]

struct_pa_sample_spec._fields_ = [
    ('format', ctypes.c_int),
    ('rate', ctypes.c_uint32),
    ('channels', ctypes.c_uint8),
]


## Wave generator
sample_rate = 44100

baudrate  = 44100 // 16  #
sig_waves = 4    # Waves/bit

phase_off =  (tau / 360) * 15  # Wave phase offset

####
####

samples_per_bit = int(sample_rate / baudrate)
sig_freq = sample_rate / (samples_per_bit / sig_waves)  # carrier freq

def genwave(freq):
    nsamples = samples_per_bit
    out = bytearray(nsamples * 2)

    for sample_i in range(nsamples):
        global sample_rate
        angle = sample_i / sample_rate * freq * tau
        s = int(sin(angle + phase_off) * 32768)

        out[sample_i *2 +0] = s & 0x00ff
        out[sample_i *2 +1] = (s & 0xff00) >> 8

    return bytes(out)

zero_wave = genwave(sig_freq)  # logical zero
one_wave  = b"\x00\x00" * samples_per_bit  # logical one

def gen_serial_byte(byte):
    out = zero_wave  # start

    for i in range(8):
        out = out + (one_wave if ((byte >> i) & 1) else zero_wave)

    out += one_wave + one_wave  # two stop bits to improve stability
    return out


pa_stream = None

def init():
    # Connection to pulseaudio server
    global pa_stream, sample_rate

    sspec = struct_pa_sample_spec()
    sspec.rate = sample_rate
    sspec.channels = 1
    sspec.format = PA_SAMPLE_S16LE

    pa_stream = _pa_simple_new(
        None,                              # default server
        b"SerialOverAudio\x00",            # stream name
        PA_STREAM_PLAYBACK,                # record/playback
        None,                              # default device
        b"Serial over audio channel\x00",  # stream description
        ctypes.byref(sspec),               # sample spec
        None,                              # default channel map
        None,                              # default buffering attributes
        None                               # ignore returned errors
    )


def send_string(bstring):
    if not pa_stream: return
    
    buf = b""
    
    for byte in bstring:
        buf += gen_serial_byte(byte)

    # write data
    _pa_simple_write(pa_stream, buf, len(buf), None)
    # flush data
    _pa_simple_drain(pa_stream, None)

