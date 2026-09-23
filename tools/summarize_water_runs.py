"""Consolida as rodadas da bancada de água num relatório único.

A decomposição por passe já é publicada em runtime pelo `[FrameProfilePasses]`;
o que faltava era juntá-la com o resultado pareado de `measure-ocean-paired`.
Sem isso, cada número mora num arquivo diferente e a comparação vira memória de
quem executou — que é exatamente o que a bancada existe para eliminar.

    python tools/summarize_water_runs.py [--out docs/measurements/<arquivo>.json]
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
VALIDATION = ROOT / "build" / "android-validation"

# Os vetores são [mean, p50, p95, p99, max].
MEDIAN = 1

PASS_KEYS = [
    ("camera_preview", "gpu_camera_preview_ms"),
    ("water_simulation", "gpu_water_simulation_ms"),
    ("shadow", "gpu_shadow_ms"),
    ("local_shadow", "gpu_local_shadow_ms"),
    ("culling", "gpu_culling_ms"),
    ("opaque", "gpu_opaque_ms"),
    ("coverage", "gpu_coverage_ms"),
    ("sky", "gpu_sky_ms"),
    ("transparent", "gpu_transparent_ms"),
    ("auto_exposure", "gpu_auto_exposure_ms"),
    ("post", "gpu_post_ms"),
    ("fsr_easu", "gpu_fsr_easu_ms"),
    ("fsr_rcas", "gpu_fsr_rcas_ms"),
    ("ui", "gpu_ui_ms"),
    ("hzb", "gpu_hzb_ms"),
]


def read_last_json_line(path: pathlib.Path, tag: str):
    """Último registro de um tag no logcat, já decodificado."""
    if not path.is_file():
        return None
    pattern = re.compile(re.escape(f"[{tag}] ") + r"(\{.*)$")
    found = None
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.search(line)
            if match:
                try:
                    found = json.loads(match.group(1))
                except json.JSONDecodeError:
                    continue
    return found


def read_passes(path: pathlib.Path, window: dict):
    """Read the matching complete pass set; reject truncated or duplicate parts."""
    if not path.is_file():
        return None
    pattern = re.compile(r"\[FrameProfilePasses\] (\{.*)$")
    pieces = {}
    legacy = None
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.search(line)
            if not match:
                continue
            try:
                record = json.loads(match.group(1))
            except json.JSONDecodeError as error:
                raise ValueError("Malformed FrameProfilePasses record") from error
            if (record.get("pid"), record.get("epoch"), record.get("window")) != (
                window.get("pid"), window.get("epoch"), window.get("window")
            ):
                continue
            if record.get("schemaVersion", 0) < 8:
                if legacy is not None or pieces:
                    raise ValueError("Duplicate FrameProfilePasses window")
                required = {key for _, key in PASS_KEYS if key in (
                    "gpu_shadow_ms", "gpu_culling_ms", "gpu_opaque_ms", "gpu_coverage_ms",
                    "gpu_sky_ms", "gpu_transparent_ms", "gpu_ui_ms", "gpu_post_ms", "gpu_hzb_ms"
                )}
                if not required.issubset(record):
                    raise ValueError("Incomplete legacy FrameProfilePasses window")
                legacy = record
                continue
            if legacy is not None or record.get("parts") != 3 or record.get("part") not in range(3):
                raise ValueError("Invalid FrameProfilePasses fragment")
            part = record["part"]
            if part in pieces:
                raise ValueError("Duplicate FrameProfilePasses fragment")
            expected = {key for _, key in PASS_KEYS[part * 5:(part + 1) * 5]}
            present = {key for _, key in PASS_KEYS if key in record}
            if present != expected:
                raise ValueError("Incomplete FrameProfilePasses fragment")
            pieces[part] = record
    if legacy is not None:
        return legacy
    if not pieces:
        return None
    if len(pieces) != 3:
        raise ValueError("Incomplete FrameProfilePasses window")
    first = pieces[0]
    for part in (1, 2):
        record = pieces[part]
        for key in ("schemaVersion", "pid", "epoch", "window", "parts", "attribution",
                    "collapsed_frames", "attribution_samples"):
            if record.get(key) != first.get(key):
                raise ValueError("Inconsistent FrameProfilePasses fragments")
        first.update({key: record[key] for _, key in PASS_KEYS[part * 5:(part + 1) * 5]})
    return first


def read_water_provider(path: pathlib.Path):
    if not path.is_file():
        return None
    pattern = re.compile(r"\[WaterFFT\] ([^\n]*)")
    found = None
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.search(line)
            if match:
                found = match.group(1).strip()
    return found


def collect_run(directory: pathlib.Path):
    summary_path = directory / "summary.json"
    if not summary_path.is_file():
        return None
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    if not summary.get("windows"):
        return None
    # Relatórios de sessões antigas trazem menos campos; a bancada é mais velha
    # que alguns dos parâmetros que ela hoje registra.
    requested = summary.get("requested", {})
    thermal = summary.get("thermalAfter", {})
    window = summary["windows"][-1]
    log = directory / "logcat.txt"
    passes = read_passes(log, window)
    memory = read_last_json_line(log, "FrameProfileMemory")

    record = {
        "run": directory.name,
        "build": window.get("build"),
        "waterIsolation": requested.get("waterIsolation"),
        "lockedCamera": requested.get("lockCamera"),
        "resolution": [window.get("width"), window.get("height")],
        "fps": round(window["present_fps"], 2),
        "gpuFrameMs": round(window["gpu_frame_ms"][MEDIAN], 3),
        "cpuProcessMs": round(window["process_cpu_ms"][MEDIAN], 3),
        "batteryCelsiusAfter": thermal.get("batteryCelsius"),
        "waterProvider": read_water_provider(log),
    }
    if passes:
        record["passesMs"] = {
            name: round(passes[key][MEDIAN], 3) for name, key in PASS_KEYS if key in passes
        }
        # O passe opaco de uma GPU de tiles engloba céu, transparente e a própria
        # água; somar tudo e comparar com o frame confirma que nada ficou fora.
        record["passesSumMs"] = round(sum(record["passesMs"].values()), 3)
    if memory and memory.get("gpu_engine_bytes"):
        record["gpuEngineMiB"] = [round(v / (1024 * 1024), 1) for v in memory["gpu_engine_bytes"]]
    return record


def collect_paired():
    reports = []
    for path in sorted(VALIDATION.glob("*-paired.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        reports.append({
            "name": data["name"],
            "isolationB": data["isolationB"],
            "baselineMeanMs": data["baselineMeanMs"],
            "baselineSdMs": data["baselineSdMs"],
            "pairedDeltaMs": data["pairedDeltaMs"],
            "pairedDeltaSdMs": data["pairedDeltaSdMs"],
            "conclusive": data["conclusive"],
        })
    return reports


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    runs = []
    for directory in sorted(VALIDATION.iterdir()):
        if directory.is_dir():
            record = collect_run(directory)
            if record:
                runs.append(record)

    report = {
        "device": "25053PC47G / SM8735 / Adreno, Android 16",
        "scene": "samples/ocean",
        "note": ("Só compare rodadas dentro de um mesmo relatório pareado. Entre "
                 "sessões separadas a mesma configuração já mediu 15,575 e 26,640 ms "
                 "com pose de câmera idêntica."),
        "paired": collect_paired(),
        "runs": runs,
    }
    text = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if arguments.out:
        out = pathlib.Path(arguments.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="utf-8")
        print(f"{len(runs)} rodadas, {len(report['paired'])} pareamentos -> {out}")
    else:
        print(text)


if __name__ == "__main__":
    main()
