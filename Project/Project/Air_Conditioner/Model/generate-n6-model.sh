#!/bin/bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
# Model dimensions, pinned source, generated headers and weight CRC are handled
# together. The old Tiny YOLOv2 model remains available as a backup reference.
python "${script_dir}/../../../../scripts/generate_model.py" "$@"
