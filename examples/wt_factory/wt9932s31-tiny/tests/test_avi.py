"""Host integration test; requires a C compiler and Pillow, no ESP-IDF or device."""

import io
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from PIL import Image


HERE = Path(__file__).resolve().parent


def riff_chunks(data, begin, end):
    """Parse generic RIFF chunks using their lengths and WORD alignment."""
    while begin < end:
        tag, size = struct.unpack_from("<4sI", data, begin)
        payload = begin + 8
        following = payload + size + (size & 1)
        if following > end:
            raise ValueError("RIFF chunk exceeds its container")
        yield tag, payload, size
        begin = following
    if begin != end:
        raise ValueError("RIFF alignment mismatch")


class AviRoundTrip(unittest.TestCase):
    def test_jpeg_padding_index_timing_and_file_safety(self):
        with tempfile.TemporaryDirectory(prefix="wt-avi-test-") as temporary:
            work = Path(temporary)
            executable = work / "roundtrip"
            subprocess.run([
                "cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-g", "-I", str(HERE.parent / "main"),
                str(HERE / "avi_roundtrip.c"), str(HERE.parent / "main/avi_writer.c"),
                "-o", str(executable),
            ], check=True)
            source = io.BytesIO()
            Image.new("RGB", (64, 48), (40, 160, 90)).save(source, "JPEG")
            original = source.getvalue()
            # A legal one-byte JPEG comment toggles length parity without
            # changing the image, exercising both RIFF padding cases.
            commented = original[:2] + b"\xff\xfe\x00\x03X" + original[2:]
            self.assertNotEqual(len(original) & 1, len(commented) & 1)
            for name, jpeg in (("original", original), ("commented", commented)):
                with self.subTest(name=name):
                    fixture = work / f"{name}.jpg"
                    fixture.write_bytes(jpeg)
                    target = work / f"{name}.avi"
                    subprocess.run([str(executable), str(fixture), str(target)], check=True)
                    data = target.read_bytes()
                    self.assertEqual(data[:4], b"RIFF")
                    self.assertEqual(data[8:12], b"AVI ")
                    self.assertEqual(struct.unpack_from("<I", data, 4)[0] + 8, len(data))
                    top = list(riff_chunks(data, 12, len(data)))
                    movi = next((p, n) for tag, p, n in top if tag == b"LIST" and data[p:p+4] == b"movi")
                    index = next((p, n) for tag, p, n in top if tag == b"idx1")
                    self.assertEqual(index[1], 5 * 16)
                    frames = list(riff_chunks(data, movi[0] + 4, movi[0] + movi[1]))
                    self.assertEqual(len(frames), 5)
                    for i, (tag, payload, size) in enumerate(frames):
                        self.assertEqual(tag, b"00dc")
                        self.assertEqual(data[payload:payload+size], jpeg)
                        decoded = Image.open(io.BytesIO(data[payload:payload+size]))
                        decoded.load()
                        self.assertEqual(decoded.size, (64, 48))
                        entry_tag, flags, offset, length = struct.unpack_from("<4sIII", data, index[0] + i * 16)
                        self.assertEqual((entry_tag, flags, length), (b"00dc", 0x10, len(jpeg)))
                        self.assertEqual(movi[0] + offset, payload - 8)
                    hdrl = next((p, n) for tag, p, n in top if tag == b"LIST" and data[p:p+4] == b"hdrl")
                    avih = next(p for tag, p, n in riff_chunks(data, hdrl[0] + 4, hdrl[0] + hdrl[1]) if tag == b"avih")
                    self.assertEqual(struct.unpack_from("<I", data, avih)[0], 42000)
                    self.assertEqual(struct.unpack_from("<I", data, avih + 16)[0], 5)


if __name__ == "__main__":
    unittest.main()
