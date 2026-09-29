import json
from pathlib import Path

out = Path(__file__).resolve().parent
d = json.loads((out/'catalogo.json').read_text(encoding='utf-8'))
raw = json.dumps(d, ensure_ascii=False).replace('<', '\\u003c')
(out/'GALERIA.html').write_text((out/'galeria.template.html').read_text(encoding='utf-8').replace('__DATA__', raw), encoding='utf-8')
s = d['stats']
lines = ['# Unity — galeria oficial de componentes, objetos e Inspector', '',
         'Consulta: 26/09/2026. Referência: Unity 6.0 (6000.0), com as versões de pacotes fixadas no inventário do projeto.', '',
         '[Abrir galeria](GALERIA.html). Use a busca, os filtros de área/pacote e o botão **Todas as imagens**. Desmarque **Com imagens** para ver todas as fichas. Clique na imagem para abrir sua resolução original.', '',
         f"São **{s['components']} fichas de componentes**, **{s['componentsWithImages']} com imagens localizadas**, e **{s['uniqueImages']} imagens oficiais distintas** contando também objetos e Inspector. Verificação HEAD: {s['imageHeadOK']} endereços responderam HTTP 200.", '',
         '## Limites da entrega', '', *['- '+x for x in d['limits']], '',
         'Não é um pacote de fotos de todos os tipos. As fichas sem imagem continuam consultáveis, com a origem do tipo. Não foram geradas imagens por IA nem realizadas capturas de uma instalação local da Unity.', '',
         '## Cobertura por pacote', '', '| Pacote | Versão no inventário | Componentes | Com imagens |', '|---|---|---:|---:|']
for m in d['packages']:
    rs = [r for r in d['records'] if r['group']=='Componentes' and r['package']==m['package']]
    if rs:
        lines.append(f"| {m['package']} | {m['version']} | {len(rs)} | {sum(bool(r['images']) for r in rs)} |")
lines += ['', '## Fontes e reprodução', '',
          '- Imagens e páginas: documentação oficial da Unity, com URL por imagem em [catalogo.json](catalogo.json).',
          '- Universo de componentes: [inventário do projeto](../../componentes/pesquisa-2026-09-15/catalogo-unity.json). A classificação inclui bases/abstratos/obsoletos/ocultos; não é uma lista de 575 itens do menu Add Component.',
          '- Coleta: `python docs/referencias/unity-visual-2026-09-26/coletar.py`; publicação: `python docs/referencias/unity-visual-2026-09-26/publicar.py`.',
          '- As imagens pertencem aos seus respectivos titulares e são apresentadas por referência remota com origem oficial.', '']
(out/'README.md').write_text('\n'.join(lines), encoding='utf-8')
print(json.dumps(s))
