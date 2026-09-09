"""Verifica entrada Astra e runtime Aether no APK, sem executar o aplicativo."""
import argparse
import os
from pathlib import Path
import subprocess
import zipfile


def verify(apk: Path, aapt: Path) -> None:
    badging = subprocess.check_output([str(aapt), 'dump', 'badging', str(apk)], text=True, encoding='utf-8')
    if "package: name='dev.aether.editor'" not in badging:
        raise ValueError('Pacote não é o aplicativo Aether')
    if "launchable-activity: name='dev.aether.editor.shell.AstraShellActivity'" not in badging:
        raise ValueError('A entrada do aplicativo não é o shell Astra')
    with zipfile.ZipFile(apk) as archive:
        names = set(archive.namelist())
        if 'lib/arm64-v8a/libaether_android.so' not in names:
            raise ValueError('Runtime Aether ARM64 ausente')
        if 'lib/arm64-v8a/libgodot_android.so' not in names:
            raise ValueError('Biblioteca da interface original Godot ausente')
        for component in ('Aether.Core.dll', 'Aether.Scene.dll', 'Aether.Rendering.dll'):
            if 'assets/dotnet/' + component not in names:
                raise ValueError('Componente gerenciado ausente: ' + component)
    print('AETHER_APK_OK: entrada Astra, runtime Aether ARM64, módulos gerenciados e biblioteca de UI Godot')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--aapt', type=Path)
    args = parser.parse_args()
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'AppData/Local/Android/Sdk')))
    candidates = sorted((sdk / 'build-tools').glob('*/aapt.exe'))
    if not args.aapt and not candidates:
        parser.error('Informe --aapt ou configure ANDROID_HOME')
    verify(args.apk, args.aapt or candidates[-1])
