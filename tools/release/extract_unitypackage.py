"""Extrai um .unitypackage para uma pasta no layout da "Pasta de pacote" da Astra.

Cada asset do pacote vira o arquivo no caminho original e, ao lado, o `.meta`
com o GUID da Unity, que é o que a Biblioteca usa para ligar prefabs, materiais e
texturas. Pastas não ganham arquivo próprio. A saída é a pasta que o build
público embute no APK com -PastraBundledPackages=<pasta-pai> (ver
android/app/build.gradle.kts, prepareBundledPackages). Os arquivos dos pacotes
nunca entram no repositório.

Uso:
  python tools/release/extract_unitypackage.py <pacote.unitypackage> <pasta-pai> <nome>
      [--exclude-ext .unity,.pdf]
"""
import argparse
import gzip
import tarfile
from pathlib import Path, PurePosixPath


def extract(package: Path, destination: Path, excluded: set[str]) -> tuple[int, int]:
    groups: dict[str, dict[str, bytes]] = {}
    with gzip.open(package, 'rb') as stream, tarfile.open(fileobj=stream, mode='r|') as archive:
        for member in archive:
            if member.isfile() and '/' in member.name:
                guid, key = member.name.split('/', 1)
                if key in ('asset', 'asset.meta', 'pathname'):
                    groups.setdefault(guid, {})[key] = archive.extractfile(member).read()
    files = written = 0
    for guid, group in sorted(groups.items()):
        if 'pathname' not in group or 'asset' not in group:
            continue  # pasta, ou entrada sem conteúdo
        pathname = group['pathname'].decode('utf-8-sig').splitlines()[0].rstrip('\x00').replace('\\', '/')
        relative = PurePosixPath(pathname)
        if relative.is_absolute() or '..' in relative.parts or ':' in pathname or '\t' in pathname:
            raise ValueError(f'Caminho inseguro no pacote: {pathname}')
        if relative.suffix.lower() in excluded:
            continue
        target = destination.joinpath(*relative.parts)
        target.resolve().relative_to(destination.resolve())
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(group['asset'])
        files += 1
        written += len(group['asset'])
        if 'asset.meta' in group:
            target.with_name(target.name + '.meta').write_bytes(group['asset.meta'])
            files += 1
    return files, written


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('package', type=Path)
    parser.add_argument('parent', type=Path)
    parser.add_argument('name')
    parser.add_argument('--exclude-ext', default='', help='extensões ignoradas, separadas por vírgula')
    args = parser.parse_args()
    destination = args.parent / args.name
    if destination.exists() and any(destination.iterdir()):
        raise SystemExit(f'{destination} já existe e não está vazia')
    destination.mkdir(parents=True, exist_ok=True)
    excluded = {e.strip().lower() for e in args.exclude_ext.split(',') if e.strip()}
    files, written = extract(args.package, destination, excluded)
    print(f'{args.name}: {files} arquivos, {written / 1048576:.1f} MiB em {destination}')


if __name__ == '__main__':
    main()
