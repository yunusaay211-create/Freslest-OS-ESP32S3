"""Freslest Screen 1: bounded stream decoder shared by viewer and tests."""
import struct
MAGIC = b'FSCRN1\r\n'
HEADER = struct.Struct('<8sIIHHI')
MAX_PIXELS = 480 * 360
MAX_PAYLOAD = MAX_PIXELS + (MAX_PIXELS + 127) // 128
SIZES = {(480, 320), (480, 360), (240, 160), (240, 180)}
RGB = tuple(bytes(((v >> 5) * 255 // 7, ((v >> 2) & 7) * 255 // 7, (v & 3) * 255 // 3)) for v in range(256))
CHANNELS = tuple(bytes(c[i] for c in RGB) for i in range(3))


def checksum(data):
    h = 2166136261
    for value in data:
        h = ((h ^ value) * 16777619) & 0xffffffff
    return h


def decode_rle(data, count):
    if not 0 < count <= MAX_PIXELS:
        raise ValueError('Invalid pixel count')
    output = bytearray()
    i = 0
    while i < len(data):
        token = data[i]
        i += 1
        n = (token & 127) + 1
        if n > count - len(output):
            raise ValueError('RLE output overflow')
        if token & 128:
            if i == len(data):
                raise ValueError('Truncated RLE run')
            output.extend(bytes((data[i],)) * n)
            i += 1
        else:
            if n > len(data) - i:
                raise ValueError('Truncated RLE literal')
            output.extend(data[i:i+n])
            i += n
    if len(output) != count:
        raise ValueError('Incomplete frame')
    return bytes(output)


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_frames = 0

    def feed(self, data):
        # Serial reads are capped by the caller; bound even an untrusted caller.
        if len(data) > 2 * MAX_PAYLOAD:
            data = data[-2 * MAX_PAYLOAD:]
        self.buffer.extend(data)
        frames = []
        while True:
            i = self.buffer.find(MAGIC)
            if i < 0:
                del self.buffer[:-7]
                break
            if i:
                del self.buffer[:i]
            if len(self.buffer) < HEADER.size:
                break
            _, length, sequence, width, height, expected = HEADER.unpack_from(self.buffer)
            if (width, height) not in SIZES or not 2 <= length <= MAX_PAYLOAD:
                del self.buffer[0]
                self.bad_frames += 1
                continue
            total = HEADER.size + length
            if len(self.buffer) < total:
                break
            payload = bytes(self.buffer[HEADER.size:total])
            try:
                if checksum(payload) != expected:
                    raise ValueError('Checksum mismatch')
                pixels = decode_rle(payload, width * height)
            except ValueError:
                # Preserve possible next magic after a truncated/noisy packet.
                del self.buffer[0]
                self.bad_frames += 1
                continue
            del self.buffer[:total]
            frames.append((sequence, width, height, pixels))
        return frames


def ppm(width, height, pixels):
    output = bytearray(len(pixels) * 3)
    for channel, table in enumerate(CHANNELS):
        output[channel::3] = pixels.translate(table)
    return f'P6\n{width} {height}\n255\n'.encode('ascii') + output
