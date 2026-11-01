import importlib.util
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "build_firmware.py"
SPEC = importlib.util.spec_from_file_location("firmware_builder_defines", SCRIPT)
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def test_compile_command_injects_extra_defines():
    previous = builder.GCC
    previous_defines = builder.EXTRA_DEFINES
    builder.GCC = Path("compiler/arm-none-eabi-gcc.exe")
    builder.EXTRA_DEFINES = ["APP_AUDIO_ES8388_SERVICE_ENABLE=1"]
    row = {"source": builder.ROOT / "test_audio_define.c",
           "object": "test_audio_define.o",
           "recipe": "arm-none-eabi-gcc \"$<\" -mcpu=cortex-m55 -o \"$@\""}
    try:
        command = builder.compile_command(row, builder.ROOT / "build/test_audio_define.o")
        assert "-DAPP_AUDIO_ES8388_SERVICE_ENABLE=1" in command
    finally:
        builder.GCC = previous
        builder.EXTRA_DEFINES = previous_defines
