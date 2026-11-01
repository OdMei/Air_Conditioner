"""Check firmware packaging guards without compiling or touching hardware."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/build_firmware.py"
SPEC = importlib.util.spec_from_file_location("firmware_builder", SCRIPT)
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def record(kind: int, address: int, data: bytes) -> str:
    body = bytes([len(data)]) + address.to_bytes(2, "big") + bytes([kind]) + data
    return ":" + (body + bytes([-sum(body) & 255])).hex().upper()


class FirmwarePackagingTests(unittest.TestCase):
    def binary(self, msp: int = 0x34200000, reset: int = 0x70100409) -> bytes:
        return struct.pack("<II", msp, reset) + bytes(range(16))

    def test_vector_stack_thumb_and_partition_guards(self):
        info = builder.validate_binary(self.binary())
        self.assertEqual(info["address"], "0x70100400")
        self.assertEqual(info["bytes"], 24)
        for malformed in (b"", b"short", self.binary(0), self.binary(0x34200004),
                          self.binary(reset=0x70100408), self.binary(reset=0x70200001)):
            with self.subTest(malformed=malformed[:8]):
                with self.assertRaises(RuntimeError):
                    builder.validate_binary(malformed)
        limit_bytes = builder.MODEL_ADDRESS - builder.VECTOR_ADDRESS
        with self.assertRaises(RuntimeError):
            builder.validate_binary(self.binary() + bytes(limit_bytes - 24))

    def test_hex_exact_payload_and_reject_holes_wrong_addresses_checksum(self):
        payload = self.binary()
        records = [record(4, 0, b"\x70\x10"), record(0, 0x0400, payload[:16]),
                   record(0, 0x0410, payload[16:]), record(1, 0, b"")]
        self.assertEqual(builder.decode_hex("\n".join(records)), payload)
        bad = [records[:-1], records + [record(0, 0x0418, b"x")],
               [records[0], record(0, 0x0401, payload), records[-1]],
               [records[0], records[1], record(0, 0x0411, payload[16:]), records[-1]],
               [records[0], records[1][:-2] + "FF", records[-1]],
               [record(4, 0, b"\x34\x00"), record(0, 0x0400, payload), records[-1]]]
        for malformed in bad:
            with self.subTest(records=malformed):
                with self.assertRaises(RuntimeError):
                    builder.decode_hex("\n".join(malformed))

    def test_empty_make_variable_does_not_consume_next_line(self):
        previous = builder.RELEASE
        with tempfile.TemporaryDirectory(prefix="n647_build_test_") as directory:
            builder.RELEASE = Path(directory)
            (builder.RELEASE / "objects.mk").write_text(
                "USER_OBJS :=\n\nLIBS := -lone -l:two.a\n", encoding="utf-8")
            try:
                self.assertEqual(builder.make_variable("USER_OBJS"), [])
                self.assertEqual(builder.make_variable("LIBS"), ["-lone", "-l:two.a"])
            finally:
                builder.RELEASE = previous

    def test_recipe_relocation_and_assembler_option_order(self):
        previous = builder.GCC
        builder.GCC = Path("compiler/arm-none-eabi-gcc.exe")
        source = builder.ROOT / "test_startup.S"
        target = builder.ROOT / "build/test_startup.o"
        row = {"source": source, "object": "test_startup.o", "recipe":
               'arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -x assembler-with-cpp '
               '-I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers" '
               '-MMD -MP -MFold.d -MT old.o -o "$@"'}
        try:
            command = builder.compile_command(row, target)
            self.assertLess(command.index("assembler-with-cpp"), command.index(source.as_posix()))
            self.assertIn("-I" + (builder.ROOT / "Drivers").as_posix(), command)
            self.assertNotIn("old.o", command)
            self.assertFalse(any("$(" in value or
                                 builder.has_unexpanded_historical_path(value)
                                 for value in command))
        finally:
            builder.GCC = previous


if __name__ == "__main__":
    unittest.main()
