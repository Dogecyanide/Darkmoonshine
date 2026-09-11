"""Execute the shared C/C++ CRC step; independent zlib and old-loop oracle."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


class Crc32Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.build = tempfile.TemporaryDirectory(prefix="lm-crc32-")
        cls.libraries = []
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
        for language, standard in (("c", "c99"), ("c++", "c++11")):
            suffix = ".dll" if os.name == "nt" else ".so"
            output = Path(cls.build.name) / ("crc-" + language.replace("+", "p") + suffix)
            command = [compiler, "-shared", "-O2", "-std=" + standard,
                       "-Wall", "-Wextra", "-Werror", "-x", language,
                       "-I", str(ROOT / "include"),
                       str(ROOT / "scripts/lm_crc32_harness.c"), "-o", str(output)]
            if os.name != "nt":
                command.insert(2, "-fPIC")
            if language == "c++":
                # C-only harness names can be mangled; compile-only verification
                # is enough to prove the exact header accepts both languages.
                command[1:2] = ["-c"]
                command[-1] = str(output.with_suffix(".o"))
            result = subprocess.run(command, capture_output=True, env=environment)
            if result.returncode:
                raise RuntimeError(result.stderr.decode(errors="replace"))
            if language != "c":
                continue
            cls.lib = ctypes.CDLL(str(output))
            cls.libraries.append(cls.lib)
            cls.lib.lm_crc_step.argtypes = [ctypes.c_uint32, ctypes.c_ubyte]
            cls.lib.lm_crc_step.restype = ctypes.c_uint32
            for name in ("lm_crc_buffer", "lm_crc_bitwise"):
                function = getattr(cls.lib, name)
                function.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
                function.restype = ctypes.c_uint32

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            for library in cls.libraries:
                FreeLibrary(library._handle)
        cls.build.cleanup()

    def check_buffer(self, payload, initial=0, offset=7):
        original = b"\xA7" * offset + payload + b"\xB9" * 37
        guarded = ctypes.create_string_buffer(original)
        pointer = ctypes.c_void_p(ctypes.addressof(guarded) + offset)
        fast = self.lib.lm_crc_buffer(pointer, len(payload), initial)
        slow = self.lib.lm_crc_bitwise(pointer, len(payload), initial)
        self.assertEqual(fast, zlib.crc32(payload, initial))
        self.assertEqual(fast, slow)
        self.assertEqual(guarded.raw, original + b"\0")
        return fast

    def test_every_byte_for_zero_ones_one_hot_and_random_running_states(self):
        randomizer = random.Random(0x4C4D4352)
        seeds = [0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA]
        seeds += [1 << i for i in range(32)]
        seeds += [randomizer.getrandbits(32) for _ in range(64)]
        for seed in seeds:
            for value in range(256):
                expected = zlib.crc32(bytes([value]), seed ^ 0xFFFFFFFF) ^ 0xFFFFFFFF
                self.assertEqual(self.lib.lm_crc_step(seed, value), expected)

    def test_standard_vectors_and_zero_length_null_pointer(self):
        self.assertEqual(self.check_buffer(b""), 0)
        self.assertEqual(self.check_buffer(b"123456789"), 0xCBF43926)
        self.assertEqual(self.check_buffer(b"The quick brown fox jumps over the lazy dog"), 0x414FA339)
        for seed in (0, 1, 0xFFFFFFFF, 0x89ABCDEF):
            self.assertEqual(self.lib.lm_crc_buffer(None, 0, seed), seed)

    def test_guarded_lengths_patterns_and_unaligned_buffers(self):
        randomizer = random.Random(0xC0DEC0DE)
        lengths = (0, 1, 2, 3, 4, 7, 8, 15, 31, 32, 33, 255, 256,
                   257, 4095, 4096, 4097, 65535, 65536, 131072, 1048576)
        for length in lengths:
            data = randomizer.randbytes(length)
            self.check_buffer(data, randomizer.getrandbits(32), offset=length % 16)
        for byte in (0, 0x55, 0xAA, 255):
            self.check_buffer(bytes([byte]) * 65537)

    def test_chunked_crc_matches_one_pass_and_zlib(self):
        randomizer = random.Random(0x53544154)
        data = randomizer.randbytes(131093)
        initial = 0xBAD5EED
        expected = zlib.crc32(data, initial)
        for chunk in (1, 3, 31, 128, 4096, 65536):
            running = initial
            for start in range(0, len(data), chunk):
                part = data[start:start + chunk]
                running = self.lib.lm_crc_buffer(part, len(part), running)
            self.assertEqual(running, expected)


if __name__ == "__main__":
    unittest.main()
