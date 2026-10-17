"""Extract supplied Unity assets safely, preserving paths, metadata and hashes."""
import argparse
from contextlib import contextmanager
import gzip
import hashlib
import json
import shutil
import tarfile
import tempfile
from pathlib import Path, PurePosixPath


SOURCE_TYPES = {'.cs', '.shader', '.cginc', '.hlsl', '.compute', '.glsl',
                '.json', '.asmdef', '.txt', '.md', '.html', '.xml', '.uxml',
                '.uss', '.mat', '.asset', '.terrainlayer', '.shadergraph',
                '.shadersubgraph', '.dll'}


def source_extract(package, root, label=None, depth=0):
    """Two sequential reads; no staging copy of textures, meshes or previews.

    Unity tar members need not put pathname before asset. Index names first,
    then copy only reference sources. Nested packages are processed one at a
    time through a bounded temporary spool and removed by its context manager.
    The manifest inventories omitted assets as omitted, never as extracted.
    """
    if depth > 8:
        raise ValueError('Nested package depth exceeds 8')
    label = label or Path(package).name
    entries = {}
    @contextmanager
    def archive():
        if hasattr(package, 'seek'):
            package.seek(0)
            compressed = gzip.GzipFile(fileobj=package, mode='rb')
        else:
            compressed = gzip.open(package, 'rb')
        # gzip handles Unity's optional header fields; tarfile's streaming
        # gzip decoder rejects some otherwise valid package headers.
        with compressed, tarfile.open(fileobj=compressed, mode='r|') as result:
            yield result
    def member_key(member):
        parts = PurePosixPath(member.name).parts
        if not member.isfile() or len(parts) != 2 or parts[1] not in ('asset', 'asset.meta', 'pathname'):
            return None
        guid, kind = parts
        if guid == 'packagemanagermanifest':
            return None
        if len(guid) != 32 or any(c not in '0123456789abcdefABCDEF' for c in guid):
            raise ValueError(f'Invalid GUID: {guid}')
        return guid, kind
    with archive() as stream:
        for member in stream:
            key = member_key(member)
            if key is None:
                continue
            guid, kind = key
            item = entries.setdefault(guid, dict(guid=guid))
            if kind == 'pathname':
                if member.size > 4096:
                    raise ValueError('Oversized pathname')
                path = stream.extractfile(member).read().decode('utf-8-sig').splitlines()[0].rstrip('\x00').replace('\\', '/')
                relative = PurePosixPath(path)
                if relative.is_absolute() or '..' in relative.parts or ':' in path or not relative.parts:
                    raise ValueError(f'Unsafe path: {path}')
                item['path'] = path
            elif kind == 'asset':
                item['bytes'] = member.size
    root = root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    paths = set()
    for item in entries.values():
        if 'path' not in item:
            raise ValueError(f'Missing pathname: {item["guid"]}')
        path = item['path']
        if path.casefold() in paths:
            raise ValueError(f'Duplicate pathname: {path}')
        paths.add(path.casefold())
        item['extracted'] = False
    with archive() as stream:
        for member in stream:
            key = member_key(member)
            if key is None:
                continue
            guid, kind = key
            item = entries[guid]
            suffix = PurePosixPath(item['path']).suffix.lower()
            if kind == 'pathname' or suffix not in SOURCE_TYPES | {'.unitypackage'}:
                continue
            if suffix != '.unitypackage' and item.get('bytes', 0) > 16 << 20:
                item['omitted_reason'] = 'Reference-source limit: asset exceeds 16 MiB'
                continue
            destination = (root / item['path']).resolve()
            if not destination.is_relative_to(root):
                raise ValueError(f'Path escapes output: {destination}')
            if suffix == '.unitypackage':
                if kind != 'asset':
                    continue
                with tempfile.SpooledTemporaryFile(max_size=8 << 20) as nested:
                    shutil.copyfileobj(stream.extractfile(member), nested, 1 << 20)
                    nested.seek(0)
                    item['sha256'] = hashlib.file_digest(nested, 'sha256').hexdigest()
                    source_extract(nested, root / '_nested' / guid,
                                   label + ' > ' + PurePosixPath(item['path']).name, depth + 1)
                    item['nested_sources'] = '_nested/' + guid
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            if kind == 'asset.meta':
                destination = Path(str(destination) + '.meta')
            digest = hashlib.sha256()
            with stream.extractfile(member) as source, destination.open('wb') as target:
                while block := source.read(1 << 20):
                    digest.update(block)
                    target.write(block)
            if kind == 'asset':
                item.update(extracted=True, sha256=digest.hexdigest())
    result = dict(package=label, mode='reference-sources', entries=list(entries.values()))
    (root / 'source-manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(package=label, entries=len(entries), extracted=sum(e['extracted'] for e in entries.values()),
                          output=str(root)), ensure_ascii=False), flush=True)


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
    parser.add_argument('--sources-only', action='store_true', help='Extract reference sources and nested sources, omitting large media')
    parser.add_argument('--package', action='append', default=[], help='Exact package basename; repeat to restrict selection')
    args = parser.parse_args()
    for package in sorted(args.source.rglob('*.unitypackage')):
        if args.package and package.name not in args.package:
            continue
        if args.sources_only:
            source_extract(package, args.output / package.stem)
        else:
            extract(package, args.output)
