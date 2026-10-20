"""Check changed terrain/transfer TUs against the configured Android toolchain.

This is a compile check, not an APK build or device/runtime validation.
Usage: python tools/check-terrain-android-sources.py <compile_commands.json>
"""
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

def command_arguments(command):
    if os.name != 'nt':
        return shlex.split(command)
    import ctypes
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    argv = parse(command, ctypes.byref(count))
    if not argv:
        raise ValueError('Invalid Windows compilation command')
    try:
        return [argv[i] for i in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(argv, ctypes.c_void_p))

targets = {
    'terrain_asset.cpp', 'editor_terrain.cpp', 'editor_terrain_screen.cpp',
    'upload_context.cpp', 'dirt_road_resources.cpp', 'instanced_renderer.cpp',
    'terrain_history_store.cpp', 'terrain_neighbors.cpp',
    'terrain_surface_asset.cpp', 'terrain_surface_catalog.cpp',
    'gltf_import.cpp', 'import_cache.cpp', 'editor_session.cpp', 'android_main.cpp',
}
commands = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
root = Path(__file__).resolve().parents[1]
terrain = next(entry for entry in commands if Path(entry['file']).name == 'terrain_asset.cpp')
if Path(terrain['file']).resolve() != root / 'native/resources/terrain_asset.cpp':
    raise ValueError('Compilation database belongs to another checkout')
# New sources use the same aether_assets options. A syntax check can use the
# existing target command before Gradle regenerates its compilation database;
# this does not stand in for linking/package validation.
for name in sorted(targets):
    if name.startswith('terrain_') and not any(Path(entry['file']).name == name for entry in commands):
        args = terrain.get('arguments') or command_arguments(terrain['command'])
        source = str(root / 'native/resources' / name)
        args = [source if arg == terrain['file'] else arg for arg in args]
        commands.append(dict(terrain, file=source, arguments=args))
seen = set()
for entry in commands:
    name = Path(entry['file']).name
    if name not in targets or name in seen:
        continue
    seen.add(name)
    args = entry.get('arguments') or command_arguments(entry['command'])
    cleaned = []
    index = 0
    while index < len(args):
        arg = args[index]
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            index += 2
            continue
        if arg not in ('-c', '-MD', '-MMD'):
            cleaned.append(arg)
        index += 1
    cleaned.append('-fsyntax-only')
    print(f'Android ARM64: {name}', flush=True)
    result = subprocess.run(cleaned, cwd=entry['directory'])
    if result.returncode:
        sys.exit(result.returncode)
missing = targets - seen
if missing:
    raise SystemExit(f'Missing Android compilation entries: {sorted(missing)}')
print(f'{len(seen)}/{len(targets)} Android translation units passed; no APK/runtime claim.', flush=True)
