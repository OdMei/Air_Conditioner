"""Read-only cross-check of the selected TFLite, generated NPU I/O and weights.

Uses only Python's standard library. It never imports the model, contacts the
board, or executes generated code. Real inference is a separate test.
"""
import argparse
import ast
import hashlib
import json
import math
import operator
from pathlib import Path
import re
import struct
import zlib


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class FlatTable:
    def __init__(self, data, position):
        self.data, self.position = data, position
        self.vtable = position - self.number(position, "i")

    def number(self, offset, kind):
        return struct.unpack_from("<" + kind, self.data, offset)[0]

    def field(self, index):
        entry = self.vtable + 4 + 2 * index
        if entry + 2 > self.vtable + self.number(self.vtable, "H"):
            return 0
        offset = self.number(entry, "H")
        return self.position + offset if offset else 0

    def reference(self, index):
        offset = self.field(index)
        return offset + self.number(offset, "I") if offset else 0

    def table(self, index):
        offset = self.reference(index)
        return FlatTable(self.data, offset) if offset else None

    def vector(self, index, kind):
        offset = self.reference(index)
        if not offset:
            return []
        count = self.number(offset, "I")
        return list(struct.unpack_from("<" + str(count) + kind, self.data, offset + 4))

    def table_vector(self, index):
        offset = self.reference(index)
        if not offset:
            return []
        result = []
        for number in range(self.number(offset, "I")):
            position = offset + 4 + number * 4
            result.append(FlatTable(self.data, position + self.number(position, "I")))
        return result


def tflite_io(path):
    payload = path.read_bytes()
    assert payload[4:8] == b"TFL3", "Source is not a TFLite FlatBuffer"
    model = FlatTable(payload, struct.unpack_from("<I", payload)[0])
    assert model.number(model.field(0), "I") == 3, "Unsupported TFLite schema"
    graphs = model.table_vector(2)
    assert len(graphs) == 1, "Application requires one model subgraph"
    graph = graphs[0]
    tensors = graph.table_vector(0)
    input_indexes, output_indexes = graph.vector(1, "i"), graph.vector(2, "i")
    assert len(input_indexes) == len(output_indexes) == 1, "Application requires one input/output"
    result = []
    for index in (input_indexes[0], output_indexes[0]):
        tensor = tensors[index]
        field = tensor.field(1)
        tensor_type = tensor.number(field, "B") if field else 0
        assert tensor_type in (0, 3), "Only FLOAT32/UINT8 interfaces supported"
        shape = tensor.vector(0, "i")
        assert shape and all(dimension > 0 for dimension in shape)
        quant = tensor.table(4)
        scales = quant.vector(2, "f") if quant else []
        zero_points = quant.vector(3, "q") if quant else []
        result.append({
            "shape": shape, "dtype": {0: "float32", 3: "uint8"}[tensor_type],
            "bytes": math.prod(shape) * (4 if tensor_type == 0 else 1),
            "quant_scales": scales, "zero_points": zero_points,
        })
    return result


def macros(*paths):
    definitions = {}
    for path in paths:
        source = re.sub(r"\\\r?\n", "", path.read_text(encoding="utf-8"))
        for match in re.finditer(r"(?m)^#define\s+([A-Za-z_]\w*)[ \t]+(.+)$", source):
            definitions[match[1]] = match[2].split("//", 1)[0].strip()
    return definitions


def value(name, definitions, active=None):
    active = set() if active is None else active
    assert name not in active, "Recursive macro"
    active = active | {name}
    expression = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)[uUlLfF]+\b",
                        r"\1", definitions[name])
    operations = {ast.Add: operator.add, ast.Sub: operator.sub,
                  ast.Mult: operator.mul, ast.Div: operator.truediv,
                  ast.FloorDiv: operator.floordiv}

    def evaluate(node):
        if isinstance(node, ast.Expression):
            return evaluate(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float, str)):
            return node.value
        if isinstance(node, ast.Name):
            return value(node.id, definitions, active)
        if isinstance(node, ast.BinOp) and type(node.op) in operations:
            return operations[type(node.op)](evaluate(node.left), evaluate(node.right))
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -evaluate(node.operand)
        raise AssertionError("Unsupported macro expression: " + name)
    return evaluate(ast.parse(expression, mode="eval"))


def braced(source, position):
    assert source[position] == "{"
    depth = 0
    for offset in range(position, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[position:offset + 1]
    raise AssertionError("Unclosed C declaration")


def function(source, name):
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source)
    assert match, "Missing production function: " + name
    return braced(source, match.end() - 1)


def descriptor(source, kind, interface):
    body = function(source, "LL_ATON_%s_Buffers_Info_%s" % (kind, interface))
    declaration = re.search(r"static const LL_Buffer_InfoTypeDef buff_info\[\]\s*=\s*\{", body)
    assert declaration
    first = body.index("{", declaration.end())
    item = braced(body, first)

    def field(name):
        match = re.search(r"\." + name + r"\s*=\s*([^,\n]+)", item)
        assert match, "Descriptor missing field: " + name
        return match[1].strip()

    def array(name, cast):
        symbol = field(name)
        match = re.search(r"\b" + re.escape(symbol) + r"\[\]\s*=\s*\{([^}]+)\}", body)
        assert match, "Descriptor array missing: " + symbol
        return [cast(part.strip()) for part in match[1].split(",") if part.strip()]

    shape = array("mem_shape", int)
    bits = int(field("nbits"))
    tensor_type = field("type")
    assert tensor_type in ("DataType_UINT8", "DataType_FLOAT")
    assert int(field("mem_ndims")) == len(shape)
    assert int(field("is_user_allocated")) == 1 and int(field("is_param")) == 0
    result = {
        "shape": shape, "dtype": {"DataType_UINT8": "uint8", "DataType_FLOAT": "float32"}[tensor_type],
        "nbits": bits, "bytes": int(field("offset_end")) - int(field("offset_start")),
        "quant_scales": array("scale", float) if kind == "Input" else [],
        "zero_points": array("offset", int) if kind == "Input" else [],
        "chpos_metadata": field("chpos"),
    }
    assert result["bytes"] == math.prod(shape) * bits // 8
    assert bits == (8 if result["dtype"] == "uint8" else 32)
    return result


def read_hex(path, expected_base, expected_bytes):
    result = bytearray()
    upper = 0
    records = 0
    ended = False
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        if not line:
            continue
        assert not ended, "HEX has records after EOF"
        assert line[0] == ":", "Malformed HEX record"
        record = bytes.fromhex(line[1:])
        count, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        assert len(record) == count + 5 and sum(record) % 256 == 0, "HEX record checksum/length failure at line %d" % line_number
        payload = record[4:-1]
        if kind == 0:
            assert upper + address == expected_base + len(result), "HEX contains a hole, overlap, or wrong base"
            result.extend(payload)
            assert len(result) <= expected_bytes, "HEX extends past declared payload"
        elif kind == 4:
            assert count == 2 and address == 0
            upper = int.from_bytes(payload, "big") << 16
        elif kind == 2:
            assert count == 2 and address == 0
            upper = int.from_bytes(payload, "big") << 4
        elif kind == 1:
            assert count == address == 0
            ended = True
        elif kind == 5:
            # objcopy may carry the binary section's start as HEX metadata.
            # It adds no data bytes and must not alter the decoded payload.
            assert count == 4 and address == 0
            assert int.from_bytes(payload, "big") == expected_base
        else:
            raise AssertionError("Unexpected model HEX record type %d" % kind)
        records += 1
    assert ended and len(result) == expected_bytes, "Missing HEX EOF or incomplete payload"
    return bytes(result), records


def validate(root):
    model = root / "Project/Project/Air_Conditioner/Model"
    include = root / "Project/Project/Air_Conditioner/Appli/Core/Inc"
    source = root / "Project/Project/Air_Conditioner/Appli/Core/Src"
    bundle = json.loads((model / "model-bundle.json").read_text(encoding="utf-8"))
    provenance = json.loads((model / "candidates/provenance.json").read_text(encoding="utf-8"))
    original = bundle["model_source"]
    candidate = model / "candidates" / Path(original["path"]).name
    assert candidate.is_file()
    matches = [entry for entry in provenance if entry["sha256"] == original["sha256"]]
    assert len(matches) == 1 and matches[0] == original, "Bundle source differs from recorded fixed-commit provenance"
    assert re.fullmatch(r"[0-9a-f]{40}", original["commit"])
    assert original["commit"] in original["source"]
    assert candidate.stat().st_size == original["bytes"] and digest(candidate) == original["sha256"]
    for filename, expected in bundle["generated_files"].items():
        assert digest(model / filename) == expected, "Generated source hash differs: " + filename

    network_source = (model / "network.c").read_text(encoding="utf-8")
    interface = bundle["network_interface"]
    tflite_input, tflite_output = tflite_io(candidate)
    generated_input = descriptor(network_source, "Input", interface)
    generated_output = descriptor(network_source, "Output", interface)
    for key in ("shape", "dtype", "bytes"):
        assert tflite_input[key] == generated_input[key] == bundle["input"][key], "Input contract mismatch: " + key
        assert tflite_output[key] == generated_output[key] == bundle["output"][key], "Output contract mismatch: " + key
    assert tflite_input["zero_points"] == generated_input["zero_points"] == [bundle["input"]["zero_point"]]
    assert len(tflite_input["quant_scales"]) == len(generated_input["quant_scales"]) == 1
    scales = [tflite_input["quant_scales"][0], generated_input["quant_scales"][0], bundle["input"]["quant_scale"]]
    assert all(abs(scale - 1 / 255) < 1e-8 for scale in scales)
    assert not tflite_output["quant_scales"] and not tflite_output["zero_points"]
    defines = macros(include / "app_model_config.h", include / "app_model_weights.h",
                     include / "app_config.h", model / "network.h")
    for name in ("LL_ATON_NETWORK_IN_NUM", "LL_ATON_NETWORK_OUT_NUM",
                 "LL_ATON_NETWORK_USER_ALLOCATED_INPUTS", "LL_ATON_NETWORK_USER_ALLOCATED_OUTPUTS"):
        assert value(name, defines) == 1
    assert value("LL_ATON_NETWORK_C_MODEL_NAME", defines) == interface
    assert [value(name, defines) for name in ("NN_HEIGHT", "NN_WIDTH", "NN_BPP")] == tflite_input["shape"][1:]
    assert value("APP_MODEL_INPUT_BYTES", defines) == value("LL_ATON_NETWORK_IN_1_SIZE_BYTES", defines) == tflite_input["bytes"]
    assert value("APP_MODEL_OUTPUT_CHANNELS", defines) == tflite_output["shape"][1]
    assert value("APP_MODEL_OUTPUT_BOXES", defines) == tflite_output["shape"][2]
    assert value("NN_BUFFER_OUT_SIZE", defines) == value("LL_ATON_NETWORK_OUT_1_SIZE_BYTES", defines) == tflite_output["bytes"]
    assert value("LL_ATON_NETWORK_IN_1_ALIGNMENT", defines) == value("LL_ATON_NETWORK_OUT_1_ALIGNMENT", defines) == 32
    assert tflite_input["bytes"] % 32 == tflite_output["bytes"] % 32 == 0
    assert abs(value("APP_MODEL_INPUT_QUANT_SCALE", defines) - scales[0]) < 1e-8
    assert value("APP_MODEL_INPUT_ZERO_POINT", defines) == 0
    assert value("APP_MODEL_CLASSES", defines) == 1
    assert defines["POSTPROCESS_TYPE"] == "POSTPROCESS_OD_YOLO_V8_UF"

    weights = bundle["weights"]
    base, length = int(weights["address"], 0), weights["bytes"]
    binary = model / "network_data.xSPI2.bin"
    ihex = root / "Project/Project/Air_Conditioner/Binary/network-data.hex"
    raw = binary.read_bytes()
    assert len(raw) == length > 0 and length <= 11 * 1024 * 1024
    assert base == value("APP_MODEL_WEIGHTS_ADDRESS", defines) == 0x70200000
    assert length == value("APP_MODEL_WEIGHTS_BYTES", defines)
    crc = zlib.crc32(raw) & 0xFFFFFFFF
    assert crc == int(weights["crc32"], 0) == value("APP_MODEL_WEIGHTS_CRC32", defines)
    assert base + length == int(weights["end_exclusive"], 0)
    assert digest(binary) == weights["bin_sha256"] and digest(ihex) == weights["hex_sha256"]
    decoded, record_count = read_hex(ihex, base, length)
    assert decoded == raw, "Every HEX payload byte must equal matching weights BIN"
    assert bundle["paired_programming_required"] is True

    application = (source / "app.c").read_text(encoding="utf-8")
    check = function(application, "app_model_check_generated_io")
    assert check.index("app_model_validate_io(") < check.index("app_model_crc32(")
    assert "input->mem_shape" in check and "output->mem_shape" in check
    assert "input->mem_ndims" in check and "output->mem_ndims" in check
    assert "APP_MODEL_WEIGHTS_CRC32" in check and "APP_AI_ERROR_MODEL_WEIGHTS" in check
    camera = function(application, "camera_thread_entry")
    gate = camera.index("app_model_check_generated_io()")
    failure = re.search(r"if\s*\(model_status\s*!=\s*APP_MODEL_IO_OK\)\s*\{", camera)
    assert failure and gate < failure.start() < camera.index("app_camera_init(")
    failed_block = braced(camera, failure.end() - 1)
    assert "return;" in failed_block and "APP_CAMERA_STARTUP_DONE" in failed_block
    assert camera.index("s_vision_running = 1U;") > camera.index("app_camera_nn_pipe_start(") > gate
    nn = function(application, "nn_thread_entry")
    assert nn.index("if (!s_vision_running)") < nn.index("LL_ATON_RT_Main(&NN_Instance_%s)" % interface)
    assert "LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(%s)" % interface in application
    assert "LL_ATON_Set_User_Input_Buffer_%s" % interface in nn
    assert "LL_ATON_Set_User_Output_Buffer_%s" % interface in nn
    ui = (source / "app_ui.c").read_text(encoding="utf-8")
    # The model identity remains inside diagnostics; the user removed preview
    # configuration labels and the raw/stable switch from the product screen.
    assert "APP_MODEL_DESCRIPTION" in ui
    assert "app_ui_smoothing_event" not in ui and "s_debug_model_text" not in ui

    return {
        "status": "passed", "source": original,
        "input": tflite_input, "output": tflite_output,
        "generated_input": generated_input, "generated_output": generated_output,
        "generated_source_sha256": bundle["generated_files"],
        "weights": weights, "hex_records_verified": record_count,
        "hex_exact_bin_equal": True, "startup_gate_before_camera_and_npu": True,
        "runtime_mapping_uses_physical_mem_shape": True, "ui_shared_model_labels": True,
        "accuracy_measured": False, "hardware_runtime_verified": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report", type=Path, default=Path(__file__).resolve().parents[3] / "tmp/model_upgrade_20261012_v10/test-model-bundle.json")
    args = parser.parse_args()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    try:
        report = validate(args.project_root.resolve())
    except Exception as error:
        args.report.write_text(json.dumps({"status": "failed", "error": str(error)}, indent=2), encoding="utf-8")
        raise
    args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("PASS: actual TFLite/generated physical I-O/config/provenance hashes, complete weights BIN/HEX/CRC, pre-camera/NPU startup gate and shared UI model labels.")
    print("Evidence: " + str(args.report))


if __name__ == "__main__":
    main()
