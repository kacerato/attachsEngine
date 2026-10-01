"""Report the locked Astra capability scope without inferring completion from names.

The JSON is the reviewed work ledger. This tool renders it and refuses scope drift;
finding an implementation file is never used to mark a capability complete.
"""
import collections
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
directory = root / 'docs/planos/ampliacao-2026-09-23'
catalog = directory / 'COMPONENTES.md'
ledger = directory / 'FAMILIAS-ESTADO.json'
report = directory / 'FAMILIAS-ESTADO.md'
rows = []
section = ''
for number, line in enumerate(catalog.read_text(encoding='utf-8').splitlines(), 1):
    if line.startswith('## Exemplos de composição'):
        break
    if line.startswith('## '):
        section = line[3:]
    if not line.startswith('| '):
        continue
    cells = [cell.strip() for cell in line.split('|')[1:-1]]
    if len(cells) != 4 or cells[0] in ('Capacidade', 'Capacidade Astra / referência'):
        continue
    rows.append({'id': f'F{len(rows)+1:03}', 'capacidade': cells[0],
                 'origem': f'COMPONENTES.md:{number}', 'familia': section,
                 'requisitos': cells[1], 'dependencias': cells[2],
                 'trabalho_original': cells[3]})

if not ledger.exists():
    for row in rows:
        conditional = row['id'] in ('F088', 'F089', 'F090', 'F091')
        row.update(escopo='condicional-P19' if conditional else 'obrigatorio',
                   estado='condicional' if conditional else 'auditar',
                   evidencias=[], lacunas=[], aceite={})
    ledger.write_text(json.dumps({'formato': 1, 'data_base': '2026-10-01',
        'universo': 'COMPONENTES.md, Unity 6000.0 e Godot 4.5; P19 habilitado por módulo',
        'nota': 'Uma linha é uma capacidade/família, não um tipo, API ou propriedade. Auditar não significa ausência de código.',
        'capacidades': rows}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

data = json.loads(ledger.read_text(encoding='utf-8'))
entries = data['capacidades']
if [(r['id'], r['capacidade']) for r in rows] != [(r['id'], r['capacidade']) for r in entries]:
    raise SystemExit('Scope changed: review ledger identities before reporting progress.')
valid = {'auditar', 'parcial', 'em_execucao', 'implementada_aguarda_aceite', 'concluida', 'condicional'}
for entry in entries:
    if entry['estado'] not in valid:
        raise SystemExit('Invalid state: ' + entry['id'])
    for path in entry['evidencias']:
        if not (root / path.split(':', 1)[0]).exists():
            raise SystemExit('Missing evidence path: ' + path)
    if entry['estado'] == 'concluida':
        if entry['lacunas'] or not entry['evidencias'] or not entry['aceite'] or any(value != 'passou' for value in entry['aceite'].values()):
            raise SystemExit('Completion lacks verified acceptance: ' + entry['id'])
mandatory = [e for e in entries if e['escopo'] == 'obrigatorio']
counts = collections.Counter(e['estado'] for e in mandatory)
closed = counts['concluida']
lines = ['# Saldo de capacidades Astra', '',
    f"**Universo atual: {len(entries)} linhas; {len(mandatory)} obrigatórias e {len(entries)-len(mandatory)} condicionais P19.**",
    f'**Encerradas neste registro: {closed}; ainda sem encerramento comprovado: {len(mandatory)-closed}.**', '',
    'A contagem de 44 famílias de componentes + 18 recursos do resumo original não cobre as 91 linhas atuais do catálogo. '
    'Este saldo usa as linhas efetivas, sem converter tipos, receitas ou lotes de 50 em famílias encerradas.', '',
    '`auditar` significa cobertura ainda não reconciliada, não ausência de implementação. '
    'Uma família parcial pode ter muitos consumidores funcionando. Fechar exige conferir cada requisito do catálogo, '
    'registrar o aceite e retirar todas as lacunas de implementação. A pedido do usuário, esta rodada usa evidências host/APK; '
    'a qualificação física integrada P20 permanece separada. Build e captura host não equivalem a prova física Android.', '',
    '**Estados obrigatórios:** ' + '; '.join(f'{key}: {value}' for key, value in sorted(counts.items())) + '.', '',
    '| ID | Capacidade | Estado | Evidências / lacunas |', '|---|---|---|---|']
for e in entries:
    evidence = ', '.join(f'`{p}`' for p in e['evidencias'])
    gaps = '; '.join(e['lacunas'])
    limits = '; '.join(e.get('limites_validacao', []))
    text = (evidence + (' — ' if evidence and gaps else '') + gaps) or 'Conferir requisitos, consumidores e aceite.'
    if limits: text += ' Limite: ' + limits
    lines.append(f"| {e['id']} | [{e['capacidade']}]({e['origem']}) | {e['estado']} | {text.replace('|', '/')} |")
lines += ['', 'Requisitos completos, dependências e evidências revisadas são mantidos em '
          '`FAMILIAS-ESTADO.json`; este Markdown é gerado por `tools/report-family-progress.py`. '
          'O atlas é referência, não confirmação automática de suporte.', '']
report.write_text('\n'.join(lines), encoding='utf-8')
print(f'{len(entries)} capabilities; {len(mandatory)} mandatory; {closed} verified closures; {len(mandatory)-closed} remain unclosed.')
