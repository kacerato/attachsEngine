"""Record local build evidence; device qualification is explicitly absent."""
from pathlib import Path
import hashlib
import json
import re
import zipfile
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
def digest(data):
    return hashlib.sha256(data).hexdigest()
def artifact(path):
    data = path.read_bytes()
    return {"path": path.relative_to(root).as_posix(), "bytes": len(data), "sha256": digest(data)}

apk = root / "android/app/build/outputs/apk/debug/app-debug.apk"
sdk = root / "android/app/build/managed-artifacts/bin/Astra.Scripting/release/Astra.Scripting.dll"
with zipfile.ZipFile(apk) as package:
    packaged = package.read("assets/dotnet/Astra.Scripting.dll")
if packaged != sdk.read_bytes():
    raise RuntimeError("Packaged SDK differs from the Android build output")

schemas = sum(int(n) for path in (root / "native/scene/schemas").glob("*.h")
              for n in re.findall(r"array<ComponentSchema,\s*(\d+)>", path.read_text(encoding="utf-8")))
facades = len(re.findall(r"public readonly struct \w+ : IComponentFacade", (root / "managed/Astra.Scripting/Generated/Components.g.cs").read_text(encoding="utf-8")))
filters = {}
for name in ("play_time_scale", "play_timer", "tween_delay", "script_abi", "audio_script", "generated_csharp"):
    text = (evidence / (name + ".txt")).read_text(encoding="utf-8-sig")
    result = re.search(r"(\d+)/(\d+) testes passaram", text)
    if not result or result[1] != result[2]:
        raise RuntimeError("Native evidence is missing or failed: " + name)
    filters[name] = int(result[1])
managed = (evidence / "managed-astra.txt").read_text(encoding="utf-8-sig")
result = re.search(r"(\d+) passaram, (\d+) falharam, (\d+) pulados", managed)
if not result or result[2] != "0" or result[3] != "0":
    raise RuntimeError("Managed evidence is missing or failed")
if "BUILD SUCCESSFUL" not in (evidence / "android-build.txt").read_text(encoding="utf-8-sig"):
    raise RuntimeError("Android build evidence is missing")

files = [apk, sdk, root / "build/editor-host/aether_tests.exe", root / "build/editor-host/aether_ui_preview.exe"]
files += sorted((root / "tests/fixtures/time/project/Time-20260930").rglob("*"))
files = [path for path in files if path.is_file()]
manifest = {
    "recorded_utc": datetime.now(timezone.utc).isoformat(),
    "scene_access_abi": 24,
    "catalog": {"native_schemas": schemas, "generated_facades": facades, "new_component_types_in_this_package": 0},
    "native_passed": filters, "native_total": sum(filters.values()),
    "managed_astra_passed": int(result[1]),
    "android_build": "passed", "packaged_sdk_matches_build": True,
    "packaged_sdk_sha256": digest(packaged),
    "device_install": False, "android_clr_acceptance": False, "android_visual_acceptance": False,
    "p00_p20_complete": False,
    "artifacts": [artifact(path) for path in files],
}
(evidence / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({key: manifest[key] for key in ("catalog", "native_total", "managed_astra_passed", "packaged_sdk_matches_build")}, ensure_ascii=False))
