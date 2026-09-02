"""Rebind a deterministic camera route after a verified LOD0-preserving migration.

Normal scene changes must record a new route. This tool is intentionally narrow:
it accepts only an AEMAP-3 manifest whose geometryLod metadata promises preserved
level-0 geometry and whose scene.aemap SHA-256 matches the manifest.
"""

import argparse
import hashlib
import json
import pathlib
import struct
import tempfile


ROUTE_MAGIC = 0x54524541
ROUTE_VERSION = 1
ROUTE_HEADER_SIZE = 32
ROUTE_SAMPLE_SIZE = 20
ROUTE_MAXIMUM_TICKS = 262144


def fnv1a64(payload):
    value = 14695981039346656037
    for byte in payload:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def rebind_route(payload, expected_source_fingerprint, target_fingerprint):
    if len(payload) < ROUTE_HEADER_SIZE:
        raise ValueError("camera route header is truncated")
    magic, version, header_size, reserved = struct.unpack_from("<4I", payload)
    source_fingerprint, tick_rate, tick_count = struct.unpack_from("<Q2I", payload, 16)
    if (magic, version, header_size, reserved) != (
            ROUTE_MAGIC, ROUTE_VERSION, ROUTE_HEADER_SIZE, 0):
        raise ValueError("unsupported camera route header")
    if not tick_count or tick_count > ROUTE_MAXIMUM_TICKS or \
            len(payload) != ROUTE_HEADER_SIZE + tick_count * ROUTE_SAMPLE_SIZE:
        raise ValueError("camera route length/tick count is invalid")
    if source_fingerprint != expected_source_fingerprint:
        raise ValueError("camera route source fingerprint does not match")
    if not target_fingerprint or target_fingerprint == source_fingerprint:
        raise ValueError("target fingerprint must identify a different valid scene")
    rebound = bytearray(payload)
    struct.pack_into("<Q", rebound, 16, target_fingerprint)
    return bytes(rebound), {"sourceFingerprint": f"{source_fingerprint:016x}",
                            "targetFingerprint": f"{target_fingerprint:016x}",
                            "tickRateHz": tick_rate, "tickCount": tick_count}


def validate_lod_manifest(manifest_path, aemap_path):
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    lod = manifest.get("geometryLod", {})
    if manifest.get("version") != 3 or manifest.get("format") != "AEMAP-3" or \
            lod.get("preservesLevel0") is not True:
        raise ValueError("manifest is not a LOD0-preserving AEMAP-3 migration")
    expected_hash = manifest.get("outputs", {}).get(aemap_path.name)
    actual_hash = hashlib.sha256(aemap_path.read_bytes()).hexdigest()
    if expected_hash != actual_hash:
        raise ValueError("scene.aemap does not match the migration manifest")
    return fnv1a64(aemap_path.read_bytes())


def atomic_write(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = pathlib.Path(stream.name)
            stream.write(payload)
            stream.flush()
        temporary.replace(path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--expected-source-fingerprint", required=True,
                        help="16-digit lowercase/uppercase hexadecimal fingerprint")
    parser.add_argument("--aemap", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        expected_source = int(args.expected_source_fingerprint, 16)
    except ValueError as error:
        raise ValueError("expected source fingerprint must be hexadecimal") from error
    target = validate_lod_manifest(args.manifest, args.aemap)
    rebound, report = rebind_route(args.source.read_bytes(), expected_source, target)
    atomic_write(args.out, rebound)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
