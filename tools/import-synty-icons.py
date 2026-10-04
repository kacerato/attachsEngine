"""Extract a user-owned Unity package and expose its actual icon previews.

No Unity/FBX renderer is simulated: originals retain GUID/meta relationships,
and icons/ contains the preview PNG bytes supplied in the package itself.
Keep the destination local; this script does not redistribute the asset pack.
"""
import argparse
import collections
import hashlib
import gzip
import json
from pathlib import Path, PurePosixPath
import struct
import tarfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--destination", type=Path,
                        default=Path("local-assets/synty-polygon-icons-v1.01"))
    args = parser.parse_args()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)

    def write(relative, data):
        relative = PurePosixPath(relative)
        if relative.is_absolute() or any(p in (".", "..") or ":" in p or "\\" in p for p in relative.parts):
            raise ValueError("Unsafe package path: " + str(relative))
        target = destination.joinpath(*relative.parts)
        if not target.resolve().is_relative_to(destination):
            raise ValueError("Escaping output path")
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.read_bytes() != data:
            raise ValueError("Existing local asset differs: " + str(target))
        if not target.exists():
            target.write_bytes(data)
        return str(relative)

    # Streaming avoids repeated gzip seeks and does not execute Unity contents.
    pending = {}
    assets = []
    previews = {}
    total = 0
    with gzip.open(args.package, "rb") as compressed, tarfile.open(fileobj=compressed, mode="r|") as package:
        for member in package:
            if not member.isfile():
                continue
            parts = PurePosixPath(member.name).parts
            if len(parts) == 1 and member.size <= 1024 * 1024:
                write("package/" + member.name, package.extractfile(member).read())
                continue
            if len(parts) != 2 or parts[1] not in ("asset", "asset.meta", "pathname", "preview.png"):
                raise ValueError("Unexpected package member: " + member.name)
            if member.size > 128 * 1024 * 1024:
                raise ValueError("Package member exceeds extraction budget")
            total += member.size
            if total > 512 * 1024 * 1024:
                raise ValueError("Package exceeds extraction budget")
            guid, kind = parts
            record = pending.setdefault(guid, {})
            record[kind] = package.extractfile(member).read()
            if "pathname" not in record:
                continue
            # Unity pathname records can end with a newline and exporter padding.
            name = record["pathname"].decode("utf-8-sig").splitlines()[0].rstrip("\x00")
            if "asset" in record:
                output = write("source/" + name, record["asset"])
                row = {"guid": guid, "path": name, "output": output,
                       "bytes": len(record["asset"]), "sha256": digest(record["asset"])}
                assets.append(row)
                if "asset.meta" in record:
                    write("source/" + name + ".meta", record["asset.meta"])
                if "preview.png" in record:
                    preview = record["preview.png"]
                    if preview.startswith(b"\x89PNG\r\n\x1a\n") and len(preview) >= 24:
                        width, height = struct.unpack(">II", preview[16:24])
                        row["preview"] = write("previews/" + guid + ".png", preview)
                        row["previewSize"] = [width, height]
                        previews[name] = preview
            del pending[guid]

    icons = []
    for asset in sorted(assets, key=lambda a: a["path"]):
        if not asset["path"].endswith(".prefab"):
            continue
        stem = PurePosixPath(asset["path"]).stem
        preview = previews.get(asset["path"])
        if preview is None:
            raise ValueError("Icon prefab has no actual PNG preview: " + asset["path"])
        icon = write("icons/" + stem + ".png", preview)
        model = next((a for a in assets if a["path"].endswith("/" + stem + ".fbx")), None)
        icons.append({"name": stem, "image": icon, "prefabGuid": asset["guid"],
                      "modelGuid": model["guid"] if model else None,
                      "category": stem.removeprefix("SM_Icon_").split("_")[0],
                      "sha256": digest(preview), "source": "Unity package preview, not a converted 3D mesh"})

    manifest = {"format": "ATTACHS-SYNTY-ICONS-1", "package": args.package.name,
                "packageSha256": digest(args.package.read_bytes()),
                "assetCount": len(assets), "iconCount": len(icons),
                "extensions": dict(collections.Counter(PurePosixPath(a["path"]).suffix for a in assets)),
                "assets": assets, "icons": icons}
    write("catalog.json", (json.dumps(manifest, indent=2) + "\n").encode("utf-8"))
    print(json.dumps({"destination": str(destination), "assets": len(assets),
                      "icons": len(icons), "extensions": manifest["extensions"]}))


if __name__ == "__main__":
    main()
