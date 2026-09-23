"""Fetch real CC0 Poly Haven models and pack their 1K glTF sources as GLB.

The games remain fully offline. Run this explicitly when refreshing the bundled
art; generate-highlevel-games.py reads the resulting GLBs without network use.
"""

from __future__ import annotations

import hashlib
import json
import struct
import sys
import urllib.parse
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "android/app/src/main/assets/astra/example-projects"
MODELS = {
    "quarentena": ("Barrel_01", "portable_generator", "korean_fire_extinguisher_01"),
    "resgate": ("medical_box", "rock_07", "metal_toolbox"),
    "perimetro": ("old_military_crate", "old_gas_mask", "vintage_radio_transceiver"),
    "mercado-nexus": ("plastic_crate_01", "wicker_basket_01", "CoffeeCart_01"),
    "farol-abissal": ("propane_tank", "modular_industrial_pipes_01", "Lantern_01"),
    "expresso-tita": ("vintage_suitcase", "industrial_storage_cart", "metal_tool_chest"),
}
HEADERS = {"User-Agent": "Astra-offline-example-assets/1.0"}


def get(url: str, md5: str | None = None) -> bytes:
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=90) as response:
        value = response.read()
    if md5 and hashlib.md5(value).hexdigest() != md5:
        raise ValueError(f"MD5 mismatch: {url}")
    return value


def aligned(data: bytearray):
    data.extend(b"\0" * (-len(data) % 4))


def pack_glb(source: dict, included: dict[str, bytes]) -> bytes:
    document = json.loads(json.dumps(source))
    combined = bytearray()
    offsets = []
    for buffer in document["buffers"]:
        uri = urllib.parse.unquote(buffer.pop("uri"))
        value = included[uri]
        if len(value) != buffer["byteLength"]:
            raise ValueError(f"Buffer size mismatch: {uri}")
        aligned(combined)
        offsets.append(len(combined))
        combined.extend(value)
    for view in document["bufferViews"]:
        original = view.get("buffer", 0)
        view["byteOffset"] = view.get("byteOffset", 0) + offsets[original]
        view["buffer"] = 0
    for picture in document.get("images", []):
        if "uri" not in picture:
            continue
        uri = urllib.parse.unquote(picture.pop("uri"))
        value = included[uri]
        aligned(combined)
        picture["bufferView"] = len(document["bufferViews"])
        picture["mimeType"] = "image/png" if uri.lower().endswith(".png") else "image/jpeg"
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(combined), "byteLength": len(value)})
        combined.extend(value)
    aligned(combined)
    document["buffers"] = [{"byteLength": len(combined)}]
    encoded = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    total = 12 + 8 + len(encoded) + 8 + len(combined)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(encoded), b"JSON") + encoded
            + struct.pack("<I4s", len(combined), b"BIN\0") + combined)


def fetch(model: str, destination: Path) -> dict:
    files = json.loads(get(f"https://api.polyhaven.com/files/{model}"))
    package = files["gltf"]["1k"]["gltf"]
    source = json.loads(get(package["url"], package["md5"]))
    included = {}
    for path, item in package["include"].items():
        included[path] = get(item["url"], item["md5"])
    result = pack_glb(source, included)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(result)
    return {"model": model, "page": f"https://polyhaven.com/a/{model}", "source": package["url"],
            "license": "CC0 1.0", "size": len(result), "sha256": hashlib.sha256(result).hexdigest(),
            "nodes": len(source["nodes"]), "meshes": len(source["meshes"])}


if __name__ == "__main__":
    requested = sys.argv[1:] or list(MODELS)
    unknown = [game for game in requested if game not in MODELS]
    if unknown:
        raise SystemExit("Jogos desconhecidos: " + ", ".join(unknown))
    for game in requested:
        names = MODELS[game]
        records = [fetch(name, OUT / game / "Assets" / f"{name}.glb") for name in names]
        (OUT / game / "Assets" / "FONTES-ARTE.md").write_text(
            "# Modelos reais utilizados\n\nPowered by Poly Haven. Os modelos abaixo são CC0 1.0 e foram "
            "baixados em glTF 1K, empacotados offline em GLB sem mudar geometria, UV ou materiais.\n\n"
            + "\n".join(f"- [{r['model']}]({r['page']}) — [fonte glTF]({r['source']}); "
                        f"SHA-256 do GLB: `{r['sha256']}`." for r in records) + "\n",
            encoding="utf-8")
        for record in records:
            print(f"{game}: {record['model']} {record['size']} bytes, {record['nodes']} nós")
