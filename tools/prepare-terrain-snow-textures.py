#!/usr/bin/env python3
"""Stage supplied M11/M12 originals and full-resolution RGBA PNGs (Pillow).

Defaults to ignored build output. Adds textures only; material order is untouched.
Catalog id remains the original byte hash; runtime_id names the encoded PNG hash;
GUID remains the source asset identity. GA packed normals are NOT relabeled RG.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "Packages/com.jbooth.microsplat."
SOURCES = (
    ("snow/Textures/microsplat_def_snow_diff.tga", (1024, 1024), "RGBA", {"albedo": "RGB/sRGB", "height": "A/linear"}),
    ("snow/Textures/microsplat_def_snow_normsao.tga", (1024, 1024), "RGBA", {"normal": "GA/linear/normal-filtered", "smoothness": "R/linear", "ao": "B/linear"}),
    ("snow/Textures/microsplat_def_snow_normalnoise.tga", (512, 512), "RGB", {"normal": "RGB/linear/normal-filtered"}),
    ("core/Textures/microsplat_def_perlin4.tga", (1024, 1024), "RGBA", {"sparkle_noise": "RGB/linear"}),
)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def png_bytes(image):
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", compress_level=6)
    return buffer.getvalue()


def prepare(package: Path, output: Path):
    manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
    catalog_path = output / "catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8")) if catalog_path.exists() else dict(
        version=1, default_profile="original", originals_preserved=True, optional_profiles=[], textures=[], materials=[], unresolved_dependencies=[])
    records, files = [], {}
    for suffix, dimensions, mode, roles in SOURCES:
        matches = [e for e in manifest["entries"] if e["path"] == PREFIX + suffix]
        if len(matches) != 1:
            raise ValueError(f"Expected one source: {suffix}")
        entry = matches[0]
        source_path = (package / entry["path"]).resolve(strict=True)
        if package.resolve() not in source_path.parents:
            raise ValueError("Source escapes package")
        raw = source_path.read_bytes()
        original_hash = digest(raw)
        if original_hash != entry["sha256"] or len(raw) != entry["bytes"]:
            raise ValueError(f"Original integrity failed: {suffix}")
        with Image.open(io.BytesIO(raw)) as image:
            image.load()
            if image.format != "TGA" or image.size != dimensions or image.mode != mode:
                raise ValueError(f"Unexpected original image contract: {suffix}")
            rgba = image.convert("RGBA")
            runtime_bytes = png_bytes(rgba)
            with Image.open(io.BytesIO(runtime_bytes)) as decoded:
                if decoded.mode != "RGBA" or decoded.size != dimensions or decoded.tobytes() != rgba.tobytes():
                    raise ValueError("Lossless RGBA conversion failed")
            thumbnail = rgba.copy()
            thumbnail.thumbnail((160, 160), Image.Resampling.LANCZOS)
            thumbnail_bytes = png_bytes(thumbnail)
        runtime_hash = digest(runtime_bytes)
        record = dict(id=original_hash, guid=entry["guid"], source=f"sources/{original_hash}.tga",
            source_path=entry["path"], package=manifest["package"], archive_sha256=manifest["sha256"],
            source_sha256=original_hash, runtime=f"textures/{original_hash}.png", runtime_sha256=runtime_hash,
            dimensions=list(dimensions), source_mode=mode, runtime_mode="RGBA",
            thumbnail=f"thumbnails/{original_hash}.png", profiles={}, runtime_id=runtime_hash,
            source_roles=roles, packing="normal=GA;smoothness=R;ao=B" if "normsao" in suffix else "channels-preserved",
            normal_ga_raw_preserved="normsao" in suffix)
        same_id = [t for t in catalog["textures"] if t["id"] == original_hash]
        same_guid = [t for t in catalog["textures"] if t["guid"] == entry["guid"]]
        if len(same_id) > 1 or (same_guid and same_guid != same_id):
            raise ValueError(f"Conflicting original identity: {suffix}")
        if same_id:
            existing = same_id[0]
            for key, value in record.items():
                if key in existing and existing[key] != value:
                    raise ValueError(f"Existing record conflicts at {key}: {suffix}")
            # Preserve unrelated metadata while adding explicit role provenance.
            record = dict(existing, **record)
        records.append(record)
        files[record["source"]] = raw
        files[record["runtime"]] = runtime_bytes
        files[record["thumbnail"]] = thumbnail_bytes
    # Validate every existing file before writing any. Never silently replace it.
    for relative, raw in files.items():
        target = output / relative
        if target.exists() and target.read_bytes() != raw:
            raise ValueError(f"Existing asset differs: {target}")
    for record in records:
        existing = next((i for i, t in enumerate(catalog["textures"]) if t["id"] == record["id"]), None)
        if existing is None:
            catalog["textures"].append(record)
        else:
            catalog["textures"][existing] = record
    for relative, raw in files.items():
        target = output / relative
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
    raw_catalog = (json.dumps(catalog, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if not catalog_path.exists() or catalog_path.read_bytes() != raw_catalog:
        output.mkdir(parents=True, exist_ok=True)
        catalog_path.write_bytes(raw_catalog)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, default=ROOT / "build/terrain-packages/MicroSplat - Terrain Collection v3.9.41")
    parser.add_argument("--output", type=Path, default=ROOT / "build/terrain-snow-staging")
    args = parser.parse_args()
    records = prepare(args.package, args.output)
    print(json.dumps(dict(originals=4, resized_originals=0, resolution_profile="original", output=str(args.output),
        assets=[dict(guid=t["guid"], original_id=t["id"], runtime_id=t["runtime_id"], packing=t["packing"]) for t in records]), indent=2))


if __name__ == "__main__":
    main()
