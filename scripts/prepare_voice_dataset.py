#!/usr/bin/env python3
"""Validate 16 kHz mono PCM voice clips and emit a KWS manifest."""
from __future__ import annotations

import argparse
import csv
import json
import wave
from pathlib import Path

LABELS = {
    "power": 1, "temp_up": 2, "temp_down": 3, "cool": 4, "fan_up": 5,
    "fan_1": 6, "fan_2": 7, "fan_3": 8, "fan_4": 9, "fan_5": 10,
    "turbo": 11, "swing": 12, "lamp": 13, "sleep": 14, "eco": 15,
    "noise": 0, "unknown": 0,
}


def inspect(path: Path) -> dict:
    with wave.open(str(path), "rb") as wav:
        info = {
            "sample_rate": wav.getframerate(),
            "channels": wav.getnchannels(),
            "sample_width": wav.getsampwidth(),
            "samples": wav.getnframes(),
        }
    if info["sample_rate"] != 16000 or info["channels"] != 1 or info["sample_width"] != 2:
        raise ValueError(f"{path}: expected mono 16 kHz 16-bit PCM, got {info}")
    return info


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dataset", type=Path, help="folder containing one subfolder per label")
    parser.add_argument("--output", type=Path, default=Path("build/voice-dataset"))
    args = parser.parse_args()
    rows = []
    for path in sorted(args.dataset.glob("*/*.wav")):
        label = path.parent.name.lower()
        if label not in LABELS:
            raise ValueError(f"unknown label {label!r}; use one of {', '.join(LABELS)}")
        info = inspect(path)
        rows.append({"path": str(path), "label": label, "keyword_id": LABELS[label], **info})
    if not rows:
        raise ValueError("no WAV files found")
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "manifest.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    (args.output / "labels.json").write_text(json.dumps(LABELS, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"validated {len(rows)} clips; wrote {args.output / 'manifest.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
