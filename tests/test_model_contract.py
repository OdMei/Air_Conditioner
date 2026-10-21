"""Execute the selected TFLite with deterministic inputs and check its I/O.

This checks interface/numerical sanity, not detection accuracy or board speed.
Use ST's bundled Python 3.9 and the task-local dependency overlay when needed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import tensorflow as tf


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(model, size, destination):
    model = Path(model).resolve()
    interpreter = tf.lite.Interpreter(model_path=str(model), num_threads=1)
    interpreter.allocate_tensors()
    inputs = interpreter.get_input_details()
    outputs = interpreter.get_output_details()
    assert len(inputs) == 1 and len(outputs) == 1, "Only one application input/output is supported"
    source, target = inputs[0], outputs[0]
    candidates = sum((size // stride) ** 2 for stride in (8, 16, 32))
    assert source["shape"].tolist() == [1, size, size, 3], "Camera requires NHWC RGB"
    assert source["dtype"] == np.uint8, "Camera data must be directly usable UINT8"
    scale, zero_point = source["quantization"]
    assert abs(scale - 1.0 / 255.0) < 1e-8 and zero_point == 0, "Raw RGB [0,255] must represent [0,1]"
    assert target["shape"].tolist() == [1, 5, candidates], "YOLOv8 postprocess requires planar xywh+person"
    assert target["dtype"] == np.float32, "Float YOLOv8 postprocess selected"
    assert target["quantization"] == (0.0, 0), "Float output must not require quantization"

    rng = np.random.default_rng(647256)
    x, y = np.meshgrid(np.arange(size), np.arange(size))
    gradient = np.stack((x * 255 // (size - 1), y * 255 // (size - 1),
                         (x + y) * 255 // (2 * (size - 1))), axis=-1)
    samples = {
        "black": np.zeros((1, size, size, 3), dtype=np.uint8),
        "white": np.full((1, size, size, 3), 255, dtype=np.uint8),
        "gradient": gradient[np.newaxis].astype(np.uint8),
        "seeded_noise": rng.integers(0, 256, (1, size, size, 3), dtype=np.uint8),
    }
    directory = destination / ("yolov8n_%d" % size)
    directory.mkdir(parents=True, exist_ok=True)
    sample_results = []
    for name, tensor in samples.items():
        interpreter.set_tensor(source["index"], tensor)
        interpreter.invoke()
        result = interpreter.get_tensor(target["index"])
        assert result.shape == (1, 5, candidates) and result.nbytes == 5 * candidates * 4
        assert np.isfinite(result).all(), "Network produced nonfinite output"
        scores = result[0, 4]
        assert (scores >= 0).all() and (scores <= 1).all(), "Confidence plane is not probabilities"
        assert (result[0, 2:4] > 0).all(), "Decoded xywh dimensions must be positive"
        # Reject pixel units: the application mapper takes normalized xywh.
        # This is numerical sanity of these inputs, not universal accuracy.
        assert float(np.max(np.abs(result[0, :4]))) < 4.0, "Decoded box planes look like pixel coordinates"
        interpreter.set_tensor(source["index"], tensor)
        interpreter.invoke()
        repeat = interpreter.get_tensor(target["index"])
        assert np.array_equal(result, repeat), "Repeated input must yield the same deterministic output"
        np.save(directory / (name + "-input.npy"), tensor, allow_pickle=False)
        output_path = directory / (name + "-output.npy")
        np.save(output_path, result, allow_pickle=False)
        result.astype("<f4").tofile(directory / (name + "-output.f32"))
        sample_results.append({
            "sample": name, "output_sha256": sha256(output_path),
            "confidence_min": float(scores.min()), "confidence_max": float(scores.max()),
            "candidates_above_0_35": int((scores >= 0.35).sum()),
            "box_plane_min": np.min(result[0, :4], axis=1).tolist(),
            "box_plane_max": np.max(result[0, :4], axis=1).tolist(),
            "repeat_exact": True,
        })
    return {
        "model": str(model), "model_sha256": sha256(model),
        "input_shape": source["shape"].tolist(), "input_dtype": "uint8",
        "input_scale": scale, "input_zero_point": zero_point,
        "input_bytes": size * size * 3,
        "output_shape": target["shape"].tolist(), "output_dtype": "float32",
        "output_bytes": candidates * 5 * 4,
        "output_layout": "planar: x_center, y_center, width, height, person_probability",
        "samples": sample_results,
        "accuracy_measured": False, "board_performance_measured": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path,
                        default=Path(__file__).resolve().parents[3] / "tmp/model_upgrade_20261012_v10/model-contract")
    parser.add_argument("--sizes", type=int, nargs="+", choices=(256, 320), default=(256, 320))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    candidates = args.project_root / "Project/Project/Air_Conditioner/Model/candidates"
    report = {
        "status": "passed", "python": sys.version, "tensorflow": tf.__version__,
        "purpose": "Real TFLite execution/I-O sanity only; synthetic inputs are not accuracy evidence",
        "models": [verify(candidates / ("yolov8n_%d_quant_pc_uf_od_coco-person.tflite" % size),
                          size, args.output_dir) for size in args.sizes],
    }
    (args.output_dir / "model-contract.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    print("PASS: %d real TFLite model(s), four deterministic inputs each, NHWC UINT8 RGB quantization, planar normalized-like FLOAT32 boxes, finite probability plane and exact repeat." % len(report["models"]))
    print("Evidence: " + str(args.output_dir / "model-contract.json"))


if __name__ == "__main__":
    main()
