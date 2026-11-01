"""Build current N647 application sources without programming or reusing objects."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
from datetime import datetime
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = Path(__file__).resolve().parents[1]
RELEASE = ROOT / "Project/Project/Air_Conditioner/STM32CubeIDE/Appli/Release"
CORE = ROOT / "Project/Project/Air_Conditioner/Appli/Core/Src"
OUTPUT = ROOT / "build/firmware"
LVGL = ROOT / "Middlewares/LVGL"
GCC = None
EXTRA_DEFINES = []
OLD_ROOT = "E:/Automation/STM32project/STM32N6/Air_Conditioner"
HISTORIC_ROOT = "D:/Learning/AC-N647/Air_Conditioner/Air_Conditioner"
VECTOR_ADDRESS = 0x70100400
MODEL_ADDRESS = 0x70200000
RAM_START, RAM_END = 0x34000000, 0x34200000
RUNTIME_BUILD = ROOT / "Middlewares/AI/Npu/runtime-build.json"


def runtime_build_config() -> dict:
    """Use the installed project's explicit runtime inventory, if present."""
    if not RUNTIME_BUILD.is_file():
        return {}
    config = json.loads(RUNTIME_BUILD.read_text(encoding="utf-8"))
    npu_root = (ROOT / "Middlewares/AI/Npu").resolve()
    for relative in config["selected_sources"]:
        source = (ROOT / relative).resolve()
        if not source.is_relative_to(npu_root) or source.suffix != ".c":
            raise RuntimeError(f"Runtime source is outside the NPU subtree: {relative}")
    if len(config["selected_sources"]) != len(set(config["selected_sources"])):
        raise RuntimeError("Duplicate sources in runtime inventory")
    library = (ROOT / config["library_path"]).resolve()
    if not library.is_relative_to((ROOT / "Middlewares/AI/Lib").resolve()) or not library.is_file():
        raise RuntimeError("Runtime inventory does not identify an installed vendor library")
    return config


def relocate(value: str) -> str:
    return value.replace("\\", "/").replace(OLD_ROOT, ROOT.as_posix()).replace(HISTORIC_ROOT, ROOT.as_posix())


def absolute_input(value: str) -> Path:
    path = Path(relocate(value))
    return (path if path.is_absolute() else RELEASE / path).resolve()


def has_unexpanded_historical_path(value: str) -> bool:
    normalized = value.replace("\\", "/")
    current_root = ROOT.as_posix().rstrip("/")
    historical_roots = (OLD_ROOT, HISTORIC_ROOT)
    return any(root.rstrip("/") != current_root and
               root.rstrip("/") in normalized for root in historical_roots)


def find_compiler(explicit: str | None = None) -> Path:
    requested = explicit or os.environ.get("ARM_GCC")
    filename = "arm-none-eabi-gcc.exe" if os.name == "nt" else "arm-none-eabi-gcc"
    if requested:
        selected = Path(requested).expanduser()
        candidates = [selected] if not selected.is_dir() else [selected / filename, selected / "bin" / filename]
        for candidate in candidates:
            if candidate.is_file():
                return candidate.resolve()
        raise RuntimeError(f"ARM_GCC/--gcc does not name an installed compiler: {requested}")
    found = shutil.which("arm-none-eabi-gcc")
    roots = []
    for variable in ("LOCALAPPDATA", "STM32_TOOLCHAIN"):
        if os.environ.get(variable):
            roots.append(Path(os.environ[variable]))
    if os.name == "nt":
        roots.extend([Path(ROOT.anchor) / "STM32-Toolchain", Path("C:/STM32-Toolchain"),
                      Path("C:/ST"), Path("C:/Program Files/STMicroelectronics")])
    patterns = ("stm32cube/bundles/gnu-tools-for-stm32/*/bin/" + filename,
                "UserData/AppData/Local/stm32cube/bundles/gnu-tools-for-stm32/*/bin/" + filename,
                "Apps/STM32CubeIDE/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/" + filename,
                "STM32CubeIDE*/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/" + filename)
    candidates = {path.resolve() for root in roots if root.is_dir()
                  for pattern in patterns for path in root.glob(pattern) if path.is_file()}
    if not candidates:
        if found:
            return Path(found).resolve()
        raise RuntimeError("ARM GCC was not found; set ARM_GCC to arm-none-eabi-gcc executable")
    # CubeIDE recipes include ST diagnostic extensions such as
    # -fcyclomatic-complexity. Prefer its installed toolchain over a generic
    # same-version Arm compiler that may happen to come first on PATH.
    return sorted(candidates, key=lambda path: path.as_posix(), reverse=True)[0]


def tool(name: str) -> Path:
    suffix = ".exe" if GCC.suffix.lower() == ".exe" else ""
    path = GCC.with_name("arm-none-eabi-" + name + suffix)
    if not path.is_file():
        raise RuntimeError(f"Missing ARM tool: {path}")
    return path


def make_variable(name: str) -> list[str]:
    text = (RELEASE / "objects.mk").read_text(encoding="utf-8").replace("\\\n", " ")
    match = re.search(r"^" + re.escape(name) + r"[ \t]*:?=[ \t]*(.*)$", text, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Missing {name} in Release/objects.mk")
    value = shlex.split(match[1], posix=True)
    if any("$(" in item for item in value):
        raise RuntimeError(f"Unexpanded {name} in Release/objects.mk")
    if name == "LIBS":
        runtime = runtime_build_config()
        if runtime:
            value = ["-l:" + runtime["library_name"] if item.startswith("-l:NetworkRuntime") else item
                     for item in value]
    return value


def link_recipe_tokens() -> list[str]:
    recipe = next(line.strip() for line in (RELEASE / "makefile").read_text().splitlines()
                  if line.strip().startswith("arm-none-eabi-gcc -o"))
    return shlex.split(recipe, posix=True)[1:]


def link_inputs() -> set[Path]:
    tokens = link_recipe_tokens()
    inputs = {RELEASE / "objects.mk"}
    libraries = [absolute_input(token[2:]) for token in tokens if token.startswith("-L")]
    inputs.update(absolute_input(token[2:]) for token in tokens if token.startswith("-T"))
    if make_variable("USER_OBJS"):
        raise RuntimeError("USER_OBJS contains prebuilt objects; add their source instead of reusing objects")
    for flag in make_variable("LIBS"):
        if not flag.startswith("-l"):
            raise RuntimeError(f"Unsupported LIBS entry: {flag}")
        filename = flag[3:] if flag.startswith("-l:") else "lib" + flag[2:] + ".a"
        path = next((directory / filename for directory in libraries if (directory / filename).is_file()), None)
        if path is None:
            raise RuntimeError(f"Missing vendor library: {filename}")
        inputs.add(path)
    if any(not path.is_file() for path in inputs):
        raise RuntimeError("Missing linker script or library input")
    return inputs


def validate_binary(binary: bytes) -> dict:
    end = VECTOR_ADDRESS + len(binary)
    if len(binary) < 8 or end >= MODEL_ADDRESS:
        raise RuntimeError("Application BIN is empty or reaches the model partition at 0x70200000")
    msp, reset = struct.unpack_from("<II", binary)
    if not (RAM_START < msp <= RAM_END and msp % 8 == 0):
        raise RuntimeError(f"Invalid initial stack pointer: {msp:#x}")
    if not (reset & 1 and VECTOR_ADDRESS <= (reset & ~1) < end):
        raise RuntimeError(f"Invalid Thumb reset vector: {reset:#x}")
    return {"address": hex(VECTOR_ADDRESS), "bytes": len(binary), "end_exclusive": hex(end),
            "last_address": hex(end - 1), "msp": hex(msp), "reset": hex(reset),
            "model_address": hex(MODEL_ADDRESS)}


def decode_hex(text: str) -> bytes:
    decoded = bytearray()
    upper = 0
    eof = False
    for line in text.splitlines():
        if not line.strip():
            continue
        if eof or not line.startswith(":"):
            raise RuntimeError("Invalid HEX framing or record after EOF")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise RuntimeError("Malformed HEX record") from error
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) % 256:
            raise RuntimeError("Invalid HEX length/checksum")
        length, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        data = record[4:4 + length]
        if kind == 0:
            if upper + address != VECTOR_ADDRESS + len(decoded):
                raise RuntimeError("HEX is not a contiguous raw image at 0x70100400")
            decoded.extend(data)
        elif kind == 1 and length == 0:
            eof = True
        elif kind == 4 and length == 2:
            upper = int.from_bytes(data, "big") << 16
        elif kind == 2 and length == 2:
            upper = int.from_bytes(data, "big") << 4
        elif kind in (3, 5) and length == 4:
            continue
        else:
            raise RuntimeError(f"Unsupported HEX record: {kind}")
    if not eof:
        raise RuntimeError("HEX has no EOF record")
    return bytes(decoded)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save_json(path: Path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def current_object_plan():
    """Resolve each historic object rule to its live source and historic flags."""
    rules = {}
    for makefile in sorted(RELEASE.rglob("subdir.mk")):
        lines = makefile.read_text(encoding="utf-8", errors="replace").splitlines()
        for index, line in enumerate(lines):
            match = re.match(r"(\S+\.o):\s+(.+?)\s+\S*subdir\.mk\s*$", line)
            if not match:
                continue
            object_name = match[1].removeprefix("./")
            command = next((item.strip() for item in lines[index + 1:index + 5]
                            if item.strip().startswith("arm-none-eabi-gcc ")), None)
            if command is None:
                raise RuntimeError(f"No compile recipe: {object_name}")
            source = absolute_input(match[2].strip().strip('"'))
            if object_name in rules:
                raise RuntimeError(f"Duplicate object rule: {object_name}")
            rules[object_name] = (source, command, makefile)
    plan = []
    seen = set()
    for line in (RELEASE / "objects.list").read_text().splitlines():
        object_name = line.strip().strip('"').removeprefix("./")
        if not object_name:
            continue
        if object_name in seen:
            raise RuntimeError(f"Duplicate object: {object_name}")
        seen.add(object_name)
        if object_name in rules:
            source, recipe, makefile = rules[object_name]
        else:
            matches = []
            for pattern, (source_pattern, command, rule_file) in rules.items():
                if "%" not in pattern:
                    continue
                prefix, suffix = pattern.split("%", 1)
                if object_name.startswith(prefix) and object_name.endswith(suffix):
                    stem = object_name[len(prefix):-len(suffix)] if suffix else object_name[len(prefix):]
                    matches.append((Path(str(source_pattern).replace("%", stem)), command, rule_file))
            if len(matches) == 1:
                source, recipe, makefile = matches[0]
            elif object_name in ("Application/User/Core/syscalls.o", "Application/User/Core/sysmem.o"):
                # CubeIDE lists these local generated C sources but omits their
                # explicit rules in the saved subdir.mk. Use the exact Core C flags.
                source = (RELEASE / ".." / Path(object_name).with_suffix(".c")).resolve()
                _, recipe, makefile = rules["Application/User/Core/app.o"]
            else:
                raise RuntimeError(f"Unmapped/ambiguous object: {object_name}")
        plan.append({"object": object_name, "source": source, "recipe": recipe,
                     "makefile": makefile, "kind": "historic-source"})
    runtime = runtime_build_config()
    if runtime:
        npu_root = (ROOT / "Middlewares/AI/Npu").resolve()
        old_npu_rows = [row for row in plan if row["source"].is_relative_to(npu_root)]
        runtime_recipe = next(row["recipe"] for row in old_npu_rows if row["source"].name == "ll_aton.c")
        seen.difference_update(row["object"] for row in old_npu_rows)
        plan = [row for row in plan if not row["source"].is_relative_to(npu_root)]
        for relative in runtime["selected_sources"]:
            source = (ROOT / relative).resolve()
            object_name = Path(relative).with_suffix(".o").as_posix()
            if object_name in seen:
                raise RuntimeError(f"Runtime object collision: {object_name}")
            plan.append({"object": object_name, "source": source, "recipe": runtime_recipe,
                         "makefile": RUNTIME_BUILD, "kind": "current-npu-runtime-source"})
            seen.add(object_name)
    core_recipe = next(row["recipe"] for row in plan if row["source"].name == "app.c")
    planned_sources = {row["source"] for row in plan}
    for source in sorted(CORE.rglob("*.c")):
        if source not in planned_sources:
            object_name = "Application/User/Core/" + source.relative_to(CORE).with_suffix(".o").as_posix()
            if object_name in seen:
                raise RuntimeError(f"Object collision for new core source: {source}")
            plan.append({"object": object_name, "source": source, "recipe": core_recipe,
                         "makefile": None, "kind": "current-core-source"})
            seen.add(object_name)
    lvgl_make = RELEASE / "Middlewares/LVGL/subdir.mk"
    lvgl_line = next(line.split(":=", 1)[1].strip() for line in lvgl_make.read_text().splitlines()
                     if line.startswith("LVGL_CFLAGS :="))
    lvgl_recipe = "arm-none-eabi-gcc " + lvgl_line.replace("$(LVGL_PATH)", LVGL.as_posix()) + ' "$<"'
    # These are platform/device backends. All other src C files are compiled;
    # disabled optional libraries remain protected by their LVGL config guards.
    excluded = ("drivers/", "draw/nxp/", "draw/renesas/", "draw/sdl/", "draw/vg_lite/")
    lvgl_excluded = []
    for source in sorted((LVGL / "src").rglob("*.c")):
        relative = source.relative_to(LVGL / "src").as_posix()
        if relative.startswith(excluded):
            lvgl_excluded.append(source)
            continue
        object_name = "Middlewares/LVGL/" + source.relative_to(LVGL).with_suffix(".o").as_posix()
        if source in {row["source"] for row in plan}:
            raise RuntimeError(f"Duplicate LVGL source: {source}")
        plan.append({"object": object_name, "source": source, "recipe": lvgl_recipe,
                     "makefile": lvgl_make, "kind": "lvgl-current-source"})
    missing = [str(row["source"]) for row in plan if not row["source"].is_file()]
    if missing:
        raise RuntimeError("Missing current sources:\n" + "\n".join(missing))
    required = {"lv_font_montserrat_20.c", "lv_font_montserrat_48.c", "app_font_ui_16.c"}
    if not required.issubset({row["source"].name for row in plan}):
        raise RuntimeError("Required UI fonts missing from full plan")
    return plan, lvgl_excluded


def compile_command(row, target: Path):
    command = [str(GCC)]
    skip_next = False
    for token in shlex.split(row["recipe"], posix=True)[1:]:
        if skip_next:
            skip_next = False
            continue
        if token in ("-o", "-MF", "-MT"):
            skip_next = True
            continue
        if token == "$<" or token in ("-MMD", "-MP") or token.startswith(("-MF", "-MT")):
            continue
        if token.startswith("-I"):
            command.append("-I" + absolute_input(token[2:]).as_posix())
        else:
            command.append(token)
    runtime = runtime_build_config()
    if runtime:
        for definition in runtime["defines"]:
            key = "-D" + definition.split("=", 1)[0]
            command = [item for item in command if item != key and not item.startswith(key + "=")]
            command.append("-D" + definition)
        for directory in runtime["include_dirs"]:
            include = "-I" + (ROOT / directory).resolve().as_posix()
            if include not in command:
                command.append(include)
    for definition in EXTRA_DEFINES:
        command.append("-D" + definition)
    command += [row["source"].as_posix(), "-o", target.as_posix(),
                "-MMD", "-MF", target.with_suffix(".d").as_posix()]
    if any("$(" in argument or has_unexpanded_historical_path(argument)
           for argument in command):
        raise RuntimeError(f"Unexpanded historical argument: {row['object']}")
    return command


def input_fingerprints(plan, commands):
    """Catch source/header changes while compiling, including external Cube headers."""
    inputs = {row["source"] for row in plan}
    inputs.add(Path(__file__).resolve())
    inputs.add(RELEASE / "objects.list")
    inputs.add(RELEASE / "makefile")
    if RUNTIME_BUILD.is_file():
        inputs.add(RUNTIME_BUILD)
        # ThreadX includes this C template directly; it is not an object.
        inputs.add(ROOT / "Middlewares/AI/Npu/ll_aton/ll_aton_osal_rtos_template.c")
    model_bundle = ROOT / "Project/Project/Air_Conditioner/Model/model-bundle.json"
    if model_bundle.is_file():
        inputs.add(model_bundle)
    inputs.update(link_inputs())
    inputs.update(row["makefile"] for row in plan if row["makefile"] is not None)
    include_dirs = set()
    for command in commands:
        include_dirs.update(Path(argument[2:]) for argument in command if argument.startswith("-I"))
    for directory in include_dirs:
        if directory.is_dir():
            inputs.update(directory.rglob("*.h"))
            inputs.update(directory.rglob("*.inc"))
    return {path.as_posix(): digest(path) for path in sorted(inputs) if path.is_file()}


def run_command(command, output: Path, log: Path):
    result = subprocess.run(command, cwd=output, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace")
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout, encoding="utf-8")
    return result.returncode, result.stdout


def link_command(output: Path, objects):
    response = output / "objects.rsp"
    response.write_text("\n".join('"' + path.as_posix() + '"' for path in objects) + "\n", encoding="utf-8")
    result = [str(GCC)]
    skip_next = False
    for token in link_recipe_tokens():
        if skip_next:
            skip_next = False
            continue
        if token == "-o":
            skip_next = True
        elif token == "$(OBJS)":
            result.append("@" + response.as_posix())
        elif token == "$(USER_OBJS)":
            continue
        elif token == "$(LIBS)":
            result.extend(make_variable("LIBS"))
        elif token.startswith(("-T", "-L")):
            result.append(token[:2] + absolute_input(token[2:]).as_posix())
        elif token.startswith("-Wl,-Map="):
            result.append("-Wl,-Map=" + (output / "appli.map").as_posix())
        elif token.startswith("-Wl,--out-implib="):
            result.append("-Wl,--out-implib=" + (output / "secure_nsclib.o").as_posix())
        else:
            result.append(token)
    result += ["-o", (output / "appli.elf").as_posix()]
    if any("$(" in argument or has_unexpanded_historical_path(argument)
           for argument in result):
        raise RuntimeError("Unexpanded link argument")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("plan", "prepare", "final"), default="plan")
    parser.add_argument("--label", help="Optional new output directory name; it must not already contain files")
    parser.add_argument("--gcc", help="ARM GCC executable or toolchain directory (or set ARM_GCC)")
    parser.add_argument("--define", action="append", default=[],
                        help="Additional compiler definition, repeatable (KEY or KEY=VALUE)")
    parser.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 1))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    global GCC, EXTRA_DEFINES
    for definition in args.define:
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*(=.*)?", definition):
            parser.error(f"--define must be KEY or KEY=VALUE: {definition}")
    EXTRA_DEFINES = list(args.define)
    GCC = find_compiler(args.gcc)
    if not GCC.is_file() or GCC.stat().st_size == 0:
        raise RuntimeError(f"Compiler missing/empty: {GCC}")
    if args.label and not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("--label must be a simple directory name")
    label = args.label or datetime.now().strftime(args.phase + "_%Y%m%d_%H%M%S")
    output = OUTPUT / label
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"Label output is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    version_code, version = run_command([str(GCC), "--version"], output, output / "compiler.log")
    if version_code:
        raise RuntimeError("ARM compiler cannot run")
    plan, excluded = current_object_plan()
    total_plan_count = len(plan)
    if args.phase == "prepare":
        plan = [row for row in plan if not row["object"].startswith(("Application/User/Core/", "Middlewares/LVGL/"))]
    commands = [compile_command(row, output / "objects" / row["object"]) for row in plan]
    manifest = {"phase": args.phase, "label": label, "status": "planned", "compiler": str(GCC),
                "programming": "not_performed",
                "compiler_version": version.splitlines()[0], "jobs": args.jobs,
                "extra_defines": EXTRA_DEFINES,
                "full_plan_count": total_plan_count, "compile_count": len(plan),
                "reused_objects": [], "excluded_platform_sources": [str(path) for path in excluded],
                "note": "Every object is compiled from the live source; Release is only read for recipes.",
                "sources": [{"path": str(row["source"]), "object": row["object"], "kind": row["kind"],
                             "command": command} for row, command in zip(plan, commands)]}
    before = input_fingerprints(plan, commands)
    manifest["input_fingerprints"] = before
    save_json(output / "manifest.json", manifest)
    print(f"PLAN {len(plan)}/{total_plan_count} current sources; {len(before)} source/header/recipe inputs; {len(excluded)} platform sources excluded", flush=True)
    if args.phase == "plan":
        return
    manifest["status"] = "compiling"
    save_json(output / "manifest.json", manifest)
    started = time.monotonic()
    failures, completed, last_progress = [], 0, started
    for row in plan:
        (output / "objects" / row["object"]).parent.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(run_command, command, output,
                               output / "logs" / Path(row["object"]).with_suffix(".log")): row
                   for row, command in zip(plan, commands)}
        for future in as_completed(futures):
            row = futures[future]
            code, stdout = future.result()
            completed += 1
            if code:
                failures.append({"object": row["object"], "exit_code": code})
                print("FAIL " + row["object"] + "\n" + "\n".join(stdout.splitlines()[-12:]), flush=True)
            if time.monotonic() - last_progress >= 10 or completed == len(plan):
                print(f"COMPILE {completed}/{len(plan)}; failures={len(failures)}; elapsed={time.monotonic()-started:.1f}s", flush=True)
                last_progress = time.monotonic()
    after = input_fingerprints(plan, commands)
    changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
    manifest["changed_inputs"] = changed
    manifest["failures"] = failures
    manifest["compile_seconds"] = time.monotonic() - started
    manifest["status"] = "compile-failed" if failures else "inputs-changed" if changed else "compiled"
    save_json(output / "manifest.json", manifest)
    if failures or changed:
        raise RuntimeError(f"Compile failures={len(failures)}, changed inputs={len(changed)}; see manifest/logs")
    if args.phase == "prepare":
        print("PASS PREPARE: no link; final will compile all live sources again after source freeze", flush=True)
        return
    objects = [output / "objects" / row["object"] for row in plan]
    command = link_command(output, objects)
    manifest["link_command"] = command
    save_json(output / "manifest.json", manifest)
    print(f"LINK {len(objects)} freshly compiled objects", flush=True)
    for label, command in [("link", command),
                           ("size", [str(tool("size")), str(output / "appli.elf")]),
                           ("undefined-symbols", [str(tool("nm")), "--undefined-only", str(output / "appli.elf")]),
                           ("objcopy-bin", [str(tool("objcopy")), "-O", "binary", str(output / "appli.elf"), str(output / "appli.bin")]),
                           ("objcopy-hex", [str(tool("objcopy")), "-I", "binary", "--change-addresses", hex(VECTOR_ADDRESS), "-O", "ihex", str(output / "appli.bin"), str(output / "appli.hex")])]:
        code, stdout = run_command(command, output, output / (label + ".log"))
        if stdout:
            print(stdout.rstrip(), flush=True)
        if code:
            manifest["status"] = label + "-failed"
            save_json(output / "manifest.json", manifest)
            raise RuntimeError(f"{label} failed: {code}")
        if label == "undefined-symbols" and stdout.strip():
            manifest["status"] = "undefined-symbols-found"
            save_json(output / "manifest.json", manifest)
            raise RuntimeError("Linked ELF still contains undefined symbols")
    binary = (output / "appli.bin").read_bytes()
    manifest["image"] = validate_binary(binary)
    decoded = decode_hex((output / "appli.hex").read_text(encoding="ascii"))
    if decoded != binary:
        raise RuntimeError("HEX payload differs from raw BIN")
    manifest["hex_source"] = "raw_binary_at_0x70100400"
    final_inputs = input_fingerprints(plan, commands)
    if before != final_inputs:
        manifest["status"] = "inputs-changed-after-link"
        save_json(output / "manifest.json", manifest)
        raise RuntimeError("Inputs changed after compile; do not use binary")
    manifest["status"] = "passed"
    manifest["binary_sha256"] = digest(output / "appli.bin")
    manifest["elf_sha256"] = digest(output / "appli.elf")
    manifest["hex_sha256"] = digest(output / "appli.hex")
    manifest["total_seconds"] = time.monotonic() - started
    save_json(output / "manifest.json", manifest)
    print("PASS " + output.as_posix(), flush=True)


if __name__ == "__main__":
    main()
