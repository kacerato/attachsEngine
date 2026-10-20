#!/usr/bin/env python3
"""Append supplied Geo PNGs to the existing catalog without changing materials.

Requires Pillow. Originals remain byte identical; thumbnails follow the library's
160-pixel policy. Run after prepare-terrain-library.py, which rebuilds the catalog.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("microsplat_def_geomap_01.png", "microsplat_def_geomap_norm_01.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, default=ROOT / "build/terrain-packages/MicroSplat - Terrain Collection v3.9.41")
    parser.add_argument("--output", type=Path, default=ROOT / "assets/terrain/library")
    args = parser.parse_args()
    manifest = json.loads((args.package / "manifest.json").read_text(encoding="utf-8"))
    catalog_path = args.output / "catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    materials_before = json.dumps(catalog["materials"], sort_keys=True)
    records = []
    # Validate/decode both sources before modifying the catalog.
    for name in NAMES:
        matches = [e for e in manifest["entries"] if e["path"] == "Packages/com.jbooth.microsplat.global-texture/Textures/" + name]
        if len(matches) != 1:
            raise ValueError(f"Expected one source for {name}")
        entry = matches[0]
        raw = (args.package / entry["path"]).read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != entry["sha256"] or len(raw) != entry["bytes"]:
            raise ValueError(f"Source integrity failed: {name}")
        with Image.open(args.package / entry["path"]) as image:
            image.load()
            if image.format != "PNG" or image.size != (1, 256):
                raise ValueError(f"Unexpected original dimensions/format: {name}")
            thumbnail = image.convert("RGBA")
            thumbnail.thumbnail((160, 160), Image.Resampling.LANCZOS)
            record = dict(id=digest, guid=entry["guid"], source=f"sources/{digest}.png",
                source_path=entry["path"], package=manifest["package"], archive_sha256=manifest["sha256"],
                source_sha256=digest, runtime=f"sources/{digest}.png", runtime_sha256=digest,
                dimensions=list(image.size), source_mode=image.mode, runtime_mode=image.mode,
                thumbnail=f"thumbnails/{digest}.png", profiles={},
                source_roles={"normal": "RGB/linear/normal-filtered"} if "norm" in name else {"albedo": "RGB/sRGB"})
            encoded = io.BytesIO()
            thumbnail.save(encoded, format="PNG")
            records.append((record, raw, encoded.getvalue()))
    # Preflight every original before any output mutation; a later conflict must
    # not leave a partially appended pair. Derived thumbnails are replaceable.
    for record, raw, _ in records:
        prior = [t for t in catalog["textures"] if t["id"] == record["id"]]
        if prior and (len(prior) != 1 or prior[0] != record):
            raise ValueError(f"Conflicting catalog record: {record['id']}")
        source = args.output / record["source"]
        if source.exists() and source.read_bytes() != raw:
            raise ValueError(f"Existing original differs: {source}")
    for record, raw, thumbnail in records:
        for relative, payload in ((record["source"], raw), (record["thumbnail"], thumbnail)):
            target = args.output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists() or target.read_bytes() != payload:
                target.write_bytes(payload)
        if not any(t["id"] == record["id"] for t in catalog["textures"]):
            catalog["textures"].append(record)
    if json.dumps(catalog["materials"], sort_keys=True) != materials_before:
        raise AssertionError("Material catalog changed")
    text = json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"
    if catalog_path.read_text(encoding="utf-8") != text:
        catalog_path.write_text(text, encoding="utf-8")
    print(f"Geo pair verified: 2 original PNGs, {len(catalog['materials'])} unchanged materials, {len(catalog['textures'])} textures")


if __name__ == "__main__":
    main()
