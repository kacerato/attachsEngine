"""Empacota a biblioteca do editor original para o host de UI Astra.

Nao substitui o APK, o launcher ou as bibliotecas Aether. A preparacao upstream
valida o commit e o SHA512 da biblioteca oficial antes de montar o AAR.
"""
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(ROOT / 'tools/prepare-godot-editor.py')], check=True)
java = ROOT / 'external/godot/platform/android/java'
subprocess.run([str(java / 'gradlew.bat'), ':lib:assembleEditorRelease', '--no-daemon'], cwd=java, check=True)
source = java / 'lib/build/outputs/aar/godot-lib.editor.aar'
destination = ROOT / 'build/godot-ui/godot-editor-ui.aar'
destination.parent.mkdir(parents=True, exist_ok=True)
temporary = destination.with_suffix('.tmp')
with zipfile.ZipFile(source) as upstream, zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as output:
    for entry in upstream.infolist():
        # O aplicativo ja fornece libc++ pelo NDK Aether. Os 131 simbolos C++
        # exigidos pelo Godot fixado estao presentes nessa biblioteca.
        if entry.filename.endswith('/libc++_shared.so'):
            continue
        output.writestr(entry, upstream.read(entry))
temporary.replace(destination)
print(destination)
