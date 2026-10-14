"""Extract supplied Unity assets safely, preserving paths, metadata and hashes."""
import argparse
import gzip
import hashlib
import json
import shutil
import tarfile
from pathlib import Path, PurePosixPath


def extract(package, output):
    root = output / package.stem
    staging = root / '.entries'
    staging.mkdir(parents=True, exist_ok=True)
    entries = {}
    with gzip.open(package, 'rb') as stream, tarfile.open(fileobj=stream, mode='r|') as archive:
        for member in archive:
            parts = PurePosixPath(member.name).parts
            if not member.isfile() or len(parts) != 2 or parts[1] not in ('asset', 'asset.meta', 'pathname', 'preview.png'):
                continue
            guid, kind = parts
            if guid == 'packagemanagermanifest':
                continue
            if len(guid) != 32 or any(c not in '0123456789abcdefABCDEF' for c in guid):
                raise ValueError(f'Invalid GUID: {guid}')
            dest = staging / guid / kind
            dest.parent.mkdir(exist_ok=True)
            with archive.extractfile(member) as source, dest.open('wb') as target:
                shutil.copyfileobj(source, target)
            entries.setdefault(guid, {})[kind] = dest
    manifest = []
    for guid, files in sorted(entries.items()):
        if 'pathname' not in files:
            raise ValueError(f'Missing pathname: {guid}')
        path = files['pathname'].read_text(encoding='utf-8-sig').splitlines()[0].rstrip('\x00').replace('\\', '/')
        relative = PurePosixPath(path)
        if relative.is_absolute() or '..' in relative.parts or ':' in path:
            raise ValueError(f'Unsafe path: {path}')
        dest = root / path
        item = dict(guid=guid, path=path)
        if 'asset' in files:
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(files['asset'], dest)
            item.update(bytes=dest.stat().st_size, sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
        else:
            dest.mkdir(parents=True, exist_ok=True)
        if 'asset.meta' in files:
            shutil.copyfile(files['asset.meta'], str(dest) + '.meta')
        if 'preview.png' in files:
            preview = root / '_previews' / (guid + '.png')
            preview.parent.mkdir(exist_ok=True)
            shutil.copyfile(files['preview.png'], preview)
            item['preview'] = str(preview.relative_to(root)).replace('\\', '/')
        manifest.append(item)
    result = dict(package=package.name, sha256=hashlib.sha256(package.read_bytes()).hexdigest(), entries=manifest)
    (root / 'manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(package=package.name, entries=len(manifest), files=sum('sha256' in e for e in manifest), output=str(root)), ensure_ascii=False), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    for package in sorted(args.source.rglob('*.unitypackage')):
        extract(package, args.output)
