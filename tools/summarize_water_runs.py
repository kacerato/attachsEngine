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

# Mediana: o `measure-ocean` publica cinco quantis por métrica e o terceiro é a
# mediana. Média seria pior aqui — um único quadro longo de compilação de
# pipeline desloca a média e não desloca a mediana.
MEDIAN = 2

PASS_KEYS = [
    ("water_simulation", "gpu_water_simulation_ms"),
    ("opaque", "gpu_opaque_ms"),
    ("post", "gpu_post_ms"),
    ("shadow", "gpu_shadow_ms"),
    ("culling", "gpu_culling_ms"),
    ("sky", "gpu_sky_ms"),
    ("transparent", "gpu_transparent_ms"),
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
    passes = read_last_json_line(log, "FrameProfilePasses")
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
