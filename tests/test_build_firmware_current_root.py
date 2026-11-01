import importlib.util
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "build_firmware.py"
SPEC = importlib.util.spec_from_file_location("firmware_builder_current_root", SCRIPT)
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def test_current_workspace_absolute_include_is_not_rejected():
    previous = builder.GCC
    builder.GCC = Path("compiler/arm-none-eabi-gcc.exe")
    source = builder.ROOT / "test_current_root.c"
    row = {"source": source, "object": "test_current_root.o", "recipe":
           'arm-none-eabi-gcc "$<" -mcpu=cortex-m55 '
           '-I"E:/Automation/STM32project/STM32N6/Air_Conditioner/Drivers" '
           '-MMD -MP -MFold.d -MT old.o -o "$@"'}
    try:
        builder.compile_command(row, builder.ROOT / "build/test_current_root.o")
    finally:
        builder.GCC = previous
