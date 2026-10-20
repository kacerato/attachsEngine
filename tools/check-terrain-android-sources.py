"""Check changed terrain/transfer TUs against the configured Android toolchain.

This is a compile check, not an APK build or device/runtime validation.
Usage: python tools/check-terrain-android-sources.py <compile_commands.json>
"""
import json
import argparse
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
    'terrain_asset.cpp', 'editor_terrain.cpp', 'editor_terrain_screen.cpp', 'editor_screen.cpp',
    'upload_context.cpp', 'dirt_road_resources.cpp', 'instanced_renderer.cpp',
    'terrain_history_store.cpp', 'terrain_neighbors.cpp',
    'terrain_surface_asset.cpp', 'terrain_surface_catalog.cpp',
    'terrain_surface_import.cpp', 'editor_model_publication.cpp',
    'gltf_import.cpp', 'import_cache.cpp', 'editor_session.cpp', 'android_main.cpp',
    'animation_curve.cpp', 'texture_streaming.cpp', 'texture_level_reader.cpp',
    'texture_budget.cpp', 'texture_compression.cpp',
    'terrain_fx_asset.cpp', 'terrain_fx_field.cpp', 'terrain_static_fx_import.cpp',
    'editor_terrain_fx_controller.cpp',
}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('database', type=Path)
parser.add_argument('--only', nargs='+', help='Recheck an explicit subset after the complete baseline check')
options = parser.parse_args()
if options.only:
    requested = set(options.only)
    unknown = requested - targets
    if unknown:
        raise SystemExit(f'Unknown terrain translation units: {sorted(unknown)}')
    targets = requested
commands = json.loads(options.database.read_text(encoding='utf-8'))
root = Path(__file__).resolve().parents[1]
terrain = next(entry for entry in commands if Path(entry['file']).name == 'terrain_asset.cpp')
if Path(terrain['file']).resolve() != root / 'native/resources/terrain_asset.cpp':
    raise ValueError('Compilation database belongs to another checkout')
# New sources use the same aether_assets options. A syntax check can use the
# existing target command before Gradle regenerates its compilation database;
# this does not stand in for linking/package validation.
for name in sorted(targets):
    if not any(Path(entry['file']).name == name for entry in commands):
        if name.startswith('terrain_'):
            template, directory = terrain, 'native/resources'
        elif name == 'editor_terrain_fx_controller.cpp':
            template = next(entry for entry in commands if Path(entry['file']).name == 'editor_terrain.cpp')
            directory = 'native/editor'
        else:
            continue
        args = template.get('arguments') or command_arguments(template['command'])
        source = str(root / directory / name)
        args = [source if arg == template['file'] else arg for arg in args]
        commands.append(dict(template, file=source, arguments=args))
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
