"""Generate and install the pinned model selected by app_model_config.h; never program hardware."""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
MODEL = ROOT / 'Project/Project/Air_Conditioner/Model'
CORE = ROOT / 'Project/Project/Air_Conditioner/Appli/Core'

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', default=shutil.which('stedgeai') or
                        'D:/STM32-Toolchain/Apps/STEdgeAI/4.0/Utilities/windows/stedgeai.exe')
    parser.add_argument('--objcopy', default=shutil.which('arm-none-eabi-objcopy') or
                        'D:/STM32-Toolchain/UserData/AppData/Local/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/arm-none-eabi-objcopy.exe')
    parser.add_argument('--tool-overlay', type=Path, help='Optional task-local SDK Python dependency overlay')
    parser.add_argument('--output-dir', type=Path, help='A fresh output directory for logs, generated files and backup')
    args = parser.parse_args()
    config = (CORE / 'Inc/app_model_config.h').read_text(encoding='utf-8')
    def number(name):
        match = re.search(r'^#define\s+' + name + r'\s+\((\d+)U\)', config, re.M)
        if not match:
            raise RuntimeError('Expected a literal configured dimension: ' + name)
        return int(match[1])
    width, height, channels, classes = map(number, ['APP_MODEL_INPUT_WIDTH', 'APP_MODEL_INPUT_HEIGHT',
                                                 'APP_MODEL_INPUT_CHANNELS', 'APP_MODEL_CLASSES'])
    if width != height or width not in (256, 320) or channels != 3 or classes != 1:
        raise RuntimeError('Only the pinned square YOLOv8n person models are supported')
    filename = f'yolov8n_{width}_quant_pc_uf_od_coco-person.tflite'
    provenance = next(row for row in json.loads((MODEL / 'candidates/provenance.json').read_text())
                      if Path(row['path']).name == filename)
    original = MODEL / 'candidates' / filename
    if digest(original) != provenance['sha256']:
        raise RuntimeError('Candidate model differs from its recorded source hash')
    output = (args.output_dir or ROOT / 'build/model-generation' /
              f'yolov8n_{width}_{datetime.now():%Y%m%d_%H%M%S}').resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError('Use a fresh output directory; old generation evidence is preserved')
    output.mkdir(parents=True, exist_ok=True)
    generated = output / 'generated'
    environment = os.environ.copy()
    if args.tool_overlay:
        overlay = args.tool_overlay.resolve()
        if not overlay.is_dir():
            raise RuntimeError('The requested SDK dependency overlay does not exist')
        environment['PYTHONPATH'] = str(overlay) + os.pathsep + environment.get('PYTHONPATH', '')
    version = subprocess.run([args.tool, '--version'], env=environment, capture_output=True,
                             text=True, check=True).stdout.strip()
    command = [args.tool, 'generate', '--name', 'network', '--model', str(original), '--target', 'stm32n6',
               '--no-inputs-allocation', '--no-outputs-allocation', '--st-neural-art', 'default@user_neuralart.json',
               '--workspace', str(output / 'workspace'), '--output', str(generated)]
    with (output / 'generate.log').open('w', encoding='utf-8') as log:
        subprocess.run(command, cwd=MODEL, env=environment, stdin=subprocess.DEVNULL,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    description = (generated / 'network.h').read_text()
    def generated_number(name):
        return int(re.search(r'#define\s+' + name + r'\s+\((\d+)\)', description)[1])
    boxes = sum((width // stride) * (height // stride) for stride in (8, 16, 32))
    for macro in ('IN_NUM', 'OUT_NUM', 'USER_ALLOCATED_INPUTS', 'USER_ALLOCATED_OUTPUTS'):
        if generated_number('LL_ATON_NETWORK_' + macro) != 1:
            raise RuntimeError('Generated network is not the expected single user-owned I/O interface')
    if (generated_number('LL_ATON_NETWORK_IN_1_SIZE_BYTES') != width * height * channels or
        generated_number('LL_ATON_NETWORK_OUT_1_SIZE_BYTES') != (4 + classes) * boxes * 4):
        raise RuntimeError('Generated buffer sizes do not match the configured model')
    raw = (generated / 'network_atonbuf.xSPI2.raw').read_bytes()
    if not 0 < len(raw) < 11 * 1024 * 1024:
        raise RuntimeError('Generated model does not fit the board model Flash partition')
    crc = zlib.crc32(raw)
    weights = MODEL / 'network_data.xSPI2.bin'
    ihex = ROOT / 'Project/Project/Air_Conditioner/Binary/network-data.hex'
    target_files = [MODEL / name for name in ('network.c', 'network.h', 'network_ecblobs.h')]
    target_files += [weights, ihex, MODEL / 'model-bundle.json', CORE / 'Inc/app_model_weights.h']
    for path in target_files:
        if path.is_file():
            backup = output / 'backup' / path.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, backup)
    for name in ('network.c', 'network.h', 'network_ecblobs.h'):
        shutil.copy2(generated / name, MODEL / name)
    weights.write_bytes(raw)
    subprocess.run([args.objcopy, '-I', 'binary', str(weights), '--change-addresses', '0x70200000',
                    '-O', 'ihex', str(ihex)], check=True)
    (CORE / 'Inc/app_model_weights.h').write_text(
        '#ifndef APP_MODEL_WEIGHTS_H\n#define APP_MODEL_WEIGHTS_H\n\n'
        '/* Generated from the exact matching NPU flash payload. */\n'
        '#define APP_MODEL_WEIGHTS_ADDRESS (0x70200000UL)\n'
        f'#define APP_MODEL_WEIGHTS_BYTES ({len(raw)}U)\n'
        f'#define APP_MODEL_WEIGHTS_CRC32 (0x{crc:08X}UL)\n\n#endif\n', encoding='utf-8')
    runtime = json.loads((ROOT / 'Middlewares/AI/Npu/runtime-build.json').read_text())
    provenance = dict(provenance, path=str(original))
    bundle = dict(name=f'YOLOv8n person INT8 {width}', network_interface='network',
                  generator_version=version, runtime_version=runtime['version'], model_source=provenance,
                  input=dict(shape=[1, height, width, channels], dtype='uint8', bytes=width*height*channels,
                             quant_scale=1/255, zero_point=0),
                  output=dict(shape=[1, 4+classes, boxes], dtype='float32', bytes=(4+classes)*boxes*4,
                              coordinates='normalized_centroid'),
                  weights=dict(address='0x70200000', bytes=len(raw), crc32=f'0x{crc:08X}',
                               end_exclusive=hex(0x70200000+len(raw)),
                               bin_sha256=digest(weights), hex_sha256=digest(ihex)),
                  generated_files={name:digest(MODEL/name) for name in ('network.c','network.h','network_ecblobs.h')},
                  paired_programming_required=True, hardware_runtime_verified=False)
    (MODEL / 'model-bundle.json').write_text(json.dumps(bundle, indent=2) + '\n', encoding='utf-8')
    print('Generated matching model and weights. Rebuild the application and validate the bundle before programming.')
    print('Evidence: ' + str(output))

if __name__ == '__main__':
    main()
