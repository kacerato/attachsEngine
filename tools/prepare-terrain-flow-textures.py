#!/usr/bin/env python3
"""Stage four supplied M13-M15 originals and full-resolution RGBA PNGs.

Requires Pillow. No production catalog or atlas is changed by default.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "Packages/com.jbooth.microsplat.streams/Textures/"
SOURCES = (
    ("microsplat_def_streamfoam.png", "04426e150438d48e19ee055632fedf02", (512, 512), "RGB", {"normal": "RG/linear/raw-packed", "foam": "B/linear", "stream_normal_foam": "RGB/linear/raw-packed"}),
    ("microsplat_def_raindrops.tga", "aca95408547244a2b8760b8425b5b0fc", (256, 256), "RGBA", {"drop_shape_lifetime": "R/linear", "ripple_direction": "GB/linear", "phase": "A/linear", "rain_drop_shape_direction_phase": "RGBA/linear/raw-packed"}),
    ("microsplat_def_lava_01.tga", "a6a8d659d589c4119a3afbd07e10a776", (1024, 1024), "RGBA", {"normal_distortion": "RG/linear", "heat_interpolation": "B/linear", "crust_darkening": "A/linear", "lava_distortion_heat_crust": "RGBA/linear/raw-packed"}),
    ("microsplat_def_lava_02.tga", "2d2978ad3535f45649b30977cb570eb8", (1024, 1024), "RGBA", {"normal_distortion": "RG/linear", "heat_interpolation": "B/linear", "crust_darkening": "A/linear", "lava_distortion_heat_crust": "RGBA/linear/raw-packed"}),
)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def png(image):
    result = io.BytesIO()
    image.save(result, format="PNG", compress_level=6)
    return result.getvalue()


def prepare(package: Path, output: Path):
    manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
    target_catalog = output / "catalog.json"
    catalog = json.loads(target_catalog.read_text(encoding="utf-8")) if target_catalog.exists() else dict(
        version=1, default_profile="original", originals_preserved=True, optional_profiles=[], textures=[], materials=[], unresolved_dependencies=[])
    files, records = {}, []
    for name, guid, dimensions, mode, roles in SOURCES:
        matches = [e for e in manifest["entries"] if e["path"] == PREFIX + name]
        if len(matches) != 1 or matches[0]["guid"] != guid:
            raise ValueError(f"Source identity mismatch: {name}")
        entry = matches[0]
        path = (package / entry["path"]).resolve(strict=True)
        if package.resolve() not in path.parents:
            raise ValueError("Source escapes package")
        original = path.read_bytes()
        identity = sha(original)
        if identity != entry["sha256"] or len(original) != entry["bytes"]:
            raise ValueError(f"Original integrity failed: {name}")
        with Image.open(io.BytesIO(original)) as image:
            image.load()
            if image.format != ("PNG" if name.endswith(".png") else "TGA") or image.size != dimensions or image.mode != mode:
                raise ValueError(f"Unexpected source image contract: {name}")
            rgba = image.convert("RGBA")
            runtime = png(rgba)
            with Image.open(io.BytesIO(runtime)) as decoded:
                if decoded.mode != "RGBA" or decoded.size != dimensions or decoded.tobytes() != rgba.tobytes():
                    raise ValueError("Lossless conversion failed")
            thumb = rgba.copy()
            thumb.thumbnail((160, 160), Image.Resampling.LANCZOS)
            thumbnail = png(thumb)
        record = dict(id=identity, guid=guid, source=f"sources/{identity}{path.suffix.lower()}",
            source_path=entry["path"], package=manifest["package"], archive_sha256=manifest["sha256"], source_sha256=identity,
            runtime=f"textures/{identity}.png", runtime_sha256=sha(runtime), runtime_id=sha(runtime),
            dimensions=list(dimensions), source_mode=mode, runtime_mode="RGBA", thumbnail=f"thumbnails/{identity}.png",
            profiles={}, source_roles=roles, packing="channels-preserved", color_space="linear", raw_channels_preserved=True)
        same_id = [t for t in catalog["textures"] if t["id"] == identity]
        same_guid = [t for t in catalog["textures"] if t["guid"] == guid]
        if len(same_id) > 1 or (same_guid and same_guid != same_id):
            raise ValueError(f"Conflicting source identity: {name}")
        if same_id:
            for key, value in record.items():
                if key in same_id[0] and same_id[0][key] != value:
                    if key == 'source_roles':
                        old_roles = same_id[0][key]
                        corrected = dict(old_roles)
                        if name == 'microsplat_def_streamfoam.png' and corrected.get('normal') == 'RG/linear/normal-filtered':
                            corrected['normal'] = 'RG/linear/raw-packed'
                        typed = 'stream_normal_foam' if name == 'microsplat_def_streamfoam.png' else 'rain_drop_shape_direction_phase' if name == 'microsplat_def_raindrops.tga' else 'lava_distortion_heat_crust'
                        if typed in value and typed not in corrected:
                            corrected[typed] = value[typed]
                        if corrected == value:
                            continue # Explicit metadata correction; source/runtime bytes unchanged.
                    raise ValueError(f"Existing record conflicts at {key}: {name}")
            record = dict(same_id[0], **record)
        records.append(record)
        files[record["source"]] = original
        files[record["runtime"]] = runtime
        files[record["thumbnail"]] = thumbnail
    # Preflight every collision before changing catalog or writing any asset.
    for relative, raw in files.items():
        destination = output / relative
        if destination.exists() and destination.read_bytes() != raw:
            raise ValueError(f"Existing asset differs: {destination}")
    for record in records:
        index = next((i for i, t in enumerate(catalog["textures"]) if t["id"] == record["id"]), None)
        if index is None:
            catalog["textures"].append(record)
        else:
            catalog["textures"][index] = record
    for relative, raw in files.items():
        destination = output / relative
        if not destination.exists():
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(raw)
    encoded = (json.dumps(catalog, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if not target_catalog.exists() or target_catalog.read_bytes() != encoded:
        output.mkdir(parents=True, exist_ok=True)
        target_catalog.write_bytes(encoded)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, default=ROOT / "build/terrain-packages/MicroSplat - Terrain Collection v3.9.41")
    parser.add_argument("--output", type=Path, default=ROOT / "build/terrain-flows-next/assets")
    args = parser.parse_args()
    records = prepare(args.package, args.output)
    print(json.dumps(dict(originals=len(records), output=str(args.output), resized_originals=0,
        assets=[dict(guid=t["guid"], original_id=t["id"], runtime_id=t["runtime_id"]) for t in records]), indent=2))


if __name__ == "__main__":
    main()
