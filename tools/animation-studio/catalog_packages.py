"""Read-only audit of supplied Unity packages; presence is never runtime support.

Outputs research manifests, every UMotion manual section/property, dependencies
and preview links. Originals are neither modified nor copied into the APK.
"""
import argparse
import hashlib
import html
import json
import re
from pathlib import Path


def text(fragment):
    return ' '.join(html.unescape(re.sub(r'<[^>]+>', ' ', fragment)).split())


def read_package(root, name):
    manifest = json.loads((root / f'{name}-manifesto.json').read_text(encoding='utf-8-sig'))
    entries, problems = [], []
    for entry in manifest:
        if not entry.get('sha256'):
            continue
        file = root / name / entry['path']
        item = dict(package=name, guid=entry['guid'], path=entry['path'],
                    format=file.suffix.lower(), expected_hash=entry['sha256'],
                    state='research', engine_availability='not_imported',
                    block='B6', dependencies=[])
        try:
            digest = hashlib.sha256(file.read_bytes()).hexdigest()
            item.update(hash=digest, verified=digest == entry['sha256'])
            if not item['verified']:
                problems.append(f'{name}/{entry["path"]}: hash mismatch')
            if file.suffix.lower() in {'.mat', '.anim', '.asset', '.prefab', '.controller', '.unity', '.meta'}:
                source = file.read_text(encoding='utf-8-sig', errors='replace')
                item['dependencies'] = sorted(set(re.findall(r'guid:\s*([0-9a-fA-F]{32})', source)))
        except OSError as error:
            item.update(verified=False, error=str(error))
            problems.append(f'{name}/{entry["path"]}: unreadable')
        preview = entry.get('package_preview')
        if preview:
            item['preview'] = preview.replace('\\', '/')
            item['preview_exists'] = (root / item['preview']).is_file()
        entries.append(item)
    known = {entry['guid'] for entry in manifest}
    for item in entries:
        item['external_dependencies'] = [guid for guid in item['dependencies'] if guid not in known and guid != '0'*32]
    return entries, problems


def manuals(root):
    manual = root / 'UMotionPro-1.29p04/Assets/UMotionEditor/Manual'
    result = []
    for file in sorted(manual.glob('*.html')):
        source = file.read_text(encoding='utf-8-sig')
        main = source.split('<div class="mainContent">', 1)
        if len(main) != 2:
            raise ValueError(f'Missing manual body: {file.name}')
        body = main[1].split('<div class="mainContentFooter">', 1)[0]
        sections = []
        matches = list(re.finditer(r'<h[1-3][^>]*>(.*?)</h[1-3]>', body, re.S))
        for index, heading in enumerate(matches):
            content = body[heading.end():matches[index+1].start() if index+1 < len(matches) else len(body)]
            properties = []
            for row in re.findall(r'<tr[^>]*>(.*?)</tr>', content, re.S):
                cells = re.findall(r'<td[^>]*>(.*?)</td>', row, re.S)
                if len(cells) < 2:
                    continue
                label = text(cells[0])
                if not label:
                    label = ' / '.join(Path(image).stem for image in re.findall(r'<img[^>]+src="([^"]+)"', cells[0]))
                properties.append(dict(name=label, description=text(cells[1]), state='research'))
            sections.append(dict(id=f'umotion:{file.stem}:{index}', name=text(heading.group(1)),
                                 description=text(content), properties=properties, state='research'))
        images = sorted(set(re.findall(r'<img[^>]+src="([^"]+)"', body)))
        result.append(dict(page=file.name, sections=sections,
                           images=[dict(path=image, exists=(manual/image).is_file()) for image in images],
                           videos=re.findall(r'<iframe[^>]+src="([^"]+)"', body),
                           video_review='not_reviewed', state='research'))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path, help='Directory containing the supplied package manifests')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    packages, issues = {}, []
    for name in ('FinalIK-2.2', 'UMotionPro-1.29p04'):
        packages[name], problems = read_package(args.root, name)
        issues.extend(problems)
    manual = manuals(args.root)
    result = dict(format=1, scope='research_only', excluded=['BoZo'], packages=packages,
                  umotion_manual=manual, issues=issues)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'catalog.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    summary = dict(files=sum(len(items) for items in packages.values()),
                   verified=sum(item['verified'] for items in packages.values() for item in items),
                   manual_pages=len(manual), sections=sum(len(page['sections']) for page in manual),
                   properties=sum(len(section['properties']) for page in manual for section in page['sections']),
                   issues=issues, engine_assets_imported=0)
    (args.output/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False))
    return 1 if issues else 0


if __name__ == '__main__':
    raise SystemExit(main())
