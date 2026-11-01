import re
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / (
    "Project/Project/Air_Conditioner/Appli/Core/Src/audio_uac_output.c")


def descriptor_bytes():
    text = SOURCE.read_text(encoding="utf-8")
    block = text.split("s_uac_descriptor[] = {", 1)[1].split("};", 1)[0]
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]+)U", block))


def test_uac_descriptor_lengths_and_audio_format():
    data = descriptor_bytes()
    offset = 0
    descriptors = []
    while offset < len(data):
        length = data[offset]
        assert length >= 2
        assert offset + length <= len(data)
        descriptors.append(data[offset:offset + length])
        offset += length
    assert offset == len(data)
    assert descriptors[0][:2] == bytes((9, 4))
    assert descriptors[1][1:3] == bytes((0x24, 1))
    assert descriptors[1][5] | (descriptors[1][6] << 8) == 30
    endpoint = next(item for item in descriptors if item[:3] == bytes((9, 5, 0x81)))
    assert endpoint[4] | (endpoint[5] << 8) == 32
    fmt = next(item for item in descriptors if item[:4] == bytes((11, 0x24, 2, 1)))
    assert fmt[7] == 1
    assert fmt[8:11] == bytes((0x80, 0x3E, 0x00))
