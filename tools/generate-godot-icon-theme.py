"""Adapta os PNGs Astra ao Theme original, sem reconstruir controles Godot."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
mapping = {
    'Add': 'add', 'Node': 'object', 'Node3D': 'object', 'MeshInstance3D': 'object',
    'Camera3D': 'camera', 'DirectionalLight3D': 'sun', 'Folder': 'folder',
    'GuiTabMenuHl': 'more', 'GuiVisibilityVisible': 'eye', 'GuiVisibilityHidden': 'eye-off',
    'Undo': 'undo', 'Redo': 'redo', 'Play': 'play', 'Stop': 'stop',
}
lines = [f'[gd_resource type="Theme" load_steps={len(set(mapping.values())) * 2 + 1} format=3]', '']
for name in sorted(set(mapping.values())):
    image = Image.open(root / f'assets/astra-visual/icons/hd-v1/{name}.png').convert('RGBA')
    image = image.resize((24, 24), Image.Resampling.LANCZOS)
    values = ','.join(map(str, image.tobytes()))
    lines += [f'[sub_resource type="Image" id="Image_{name}"]',
              f'data = {{"data": PackedByteArray({values}), "format": "RGBA8", "height": 24, "mipmaps": false, "width": 24}}',
              f'[sub_resource type="ImageTexture" id="Texture_{name}"]', f'image = SubResource("Image_{name}")', '']
lines += ['[resource]']
for upstream, name in mapping.items():
    lines.append(f'EditorIcons/icons/{upstream} = SubResource("Texture_{name}")')
destination = root / 'assets/astra-visual/godot/astra-icons.tres'
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text('\n'.join(lines) + '\n', encoding='utf-8')
