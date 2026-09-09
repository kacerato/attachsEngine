"""Import GLB/glTF into independently instantiable mesh resources and a scene.
Geometry/images are shared by every mesh resource, never duplicated per object.
The output is source data; runtime cooking is a separate consumer.
"""
import argparse
import base64
import copy
import hashlib
import importlib.util
import json
import os
import tempfile
from pathlib import Path
from urllib.parse import unquote, urlsplit

SPEC = importlib.util.spec_from_file_location("map_cooker", Path(__file__).with_name("cook-gltf-map.py"))
COOK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COOK)


def atomic_write(path, data):
    """Publish a complete file on the same filesystem; preserve the old file on error."""
    fd, temporary = tempfile.mkstemp(prefix=".import-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def normalize_source(source, source_bytes):
    """Resolve a local glTF/GLB into one buffer without baking node transforms."""
    if source_bytes[:4] == b"glTF":
        document, embedded = COOK.read_glb(source_bytes)
    else:
        document, embedded = json.loads(source_bytes.decode("utf-8-sig")), b""
    if document.get("asset", {}).get("version") != "2.0":
        raise ValueError("only glTF 2.0 is supported")
    dependencies = {}
    def resolve(uri):
        if uri.startswith("data:"):
            header, separator, body = uri.partition(",")
            if not separator or not header.endswith(";base64"):
                raise ValueError("resource data URI must use base64")
            return base64.b64decode(body, validate=True)
        parsed = urlsplit(uri)
        if (parsed.scheme or parsed.netloc or parsed.query or parsed.fragment or
                Path(unquote(parsed.path)).is_absolute()):
            raise ValueError("resource must be a local relative URI")
        path = (source.parent / unquote(parsed.path)).resolve()
        if not path.is_relative_to(source.parent.resolve()):
            raise ValueError("resource escapes the import directory")
        data = path.read_bytes()
        dependencies[uri] = hashlib.sha256(data).hexdigest()
        return data
    binary = bytearray()
    offsets, lengths = [], []
    for index, buffer in enumerate(document.get("buffers", [])):
        if "uri" not in buffer and index != 0:
            raise ValueError("only the first GLB buffer can be embedded")
        data = resolve(buffer["uri"]) if "uri" in buffer else embedded
        length = buffer["byteLength"]
        if not isinstance(length, int) or length < 0 or length > len(data):
            raise ValueError("invalid buffer byteLength")
        binary.extend(b"\0" * (-len(binary) % 4))
        offsets.append(len(binary)); lengths.append(length)
        binary.extend(data[:length])
    for view in document.get("bufferViews", []):
        index, start, length = view.get("buffer", 0), view.get("byteOffset", 0), view["byteLength"]
        if (not all(isinstance(v, int) for v in (index,start,length)) or
                index < 0 or index >= len(offsets) or start < 0 or length < 0 or
                start + length > lengths[index]):
            raise ValueError("bufferView outside its source buffer")
        view["buffer"] = 0; view["byteOffset"] = offsets[index] + start
    for image in document.get("images", []):
        if "uri" not in image:
            continue
        data = resolve(image.pop("uri"))
        if data.startswith(b"\x89PNG\r\n\x1a\n"):
            mime = "image/png"
        elif data.startswith(b"\xff\xd8\xff"):
            mime = "image/jpeg"
        else:
            raise ValueError("external image must be PNG or JPEG")
        binary.extend(b"\0" * (-len(binary) % 4))
        views = document.setdefault("bufferViews", [])
        image["bufferView"] = len(views); image["mimeType"] = mime
        views.append({"buffer":0, "byteOffset":len(binary), "byteLength":len(data)})
        binary.extend(data)
    document["buffers"] = [{"byteLength":len(binary)}]
    return document, bytes(binary), dependencies


def export_assets(source, destination, source_id):
    source, destination = Path(source), Path(destination)
    source_bytes = source.read_bytes()
    document, binary, dependencies = normalize_source(source, source_bytes)
    catalog = COOK.authoring_catalog(document, source_id, {})
    resources = destination / "resources"
    # Content addressing prevents a new import from overwriting a buffer still
    # referenced by an older project revision.
    binary_name = hashlib.sha256(binary).hexdigest() + ".bin"
    pending = {binary_name: binary}
    shared = copy.deepcopy(document)
    shared["buffers"] = [{"uri": binary_name, "byteLength": len(binary)}]
    for entry in catalog["meshes"]:
        mesh_index = entry["sourceMesh"]
        mesh_document = copy.deepcopy(shared)
        mesh_document["meshes"] = [copy.deepcopy(document["meshes"][mesh_index])]
        mesh_document["nodes"] = [{"name": entry["name"], "mesh": 0}]
        mesh_document["scenes"] = [{"nodes": [0]}]
        mesh_document["scene"] = 0
        mesh_document.pop("animations", None)
        mesh_document.pop("skins", None)
        for primitive in mesh_document["meshes"][0]["primitives"]:
            if any(name.startswith("JOINTS_") or name.startswith("WEIGHTS_") for name in primitive["attributes"]):
                raise ValueError("skinned mesh requires skeleton export")
        data = json.dumps(mesh_document, ensure_ascii=False).encode("utf-8")
        # Stable asset identity is separate from immutable resource revision.
        # A saved catalog must keep referring to exactly the geometry it used.
        name = hashlib.sha256(data).hexdigest() + ".gltf"
        pending[name] = data
        entry["sourcePath"] = "resources/" + name
    catalog["sourceSha256"] = hashlib.sha256(source_bytes).hexdigest()
    catalog["dependencies"] = dependencies
    encoded = json.dumps(catalog, indent=2, ensure_ascii=False).encode("utf-8")
    # Validate every resource before the first filesystem mutation. The catalog
    # is the commit point; an interrupted import can leave only unused immutable
    # resources, never references to partially written files.
    resources.mkdir(parents=True, exist_ok=True)
    for name, data in pending.items():
        path = resources / name
        if not path.exists() or path.read_bytes() != data:
            atomic_write(path, data)
    atomic_write(destination / "scene.authoring.json", encoded)
    return catalog


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--source-id", required=True)
    args = parser.parse_args()
    catalog = export_assets(args.source, args.destination, args.source_id)
    print(f"{len(catalog['objects'])} objects, {len(catalog['meshes'])} reusable mesh resources")
