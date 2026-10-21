# Conjunto mesclado full Unity — validação (20/10/2026)

Branch `codex/ui-play-performance-apk-export` depois dos merges da auditoria
(`claude/nice-herschel-a55329`), G3a materiais (`claude/g3a-material-audit-a6b1f4`),
G4 nuvens (`claude/nostalgic-swartz-338e86`) e PK-3/PK-8 luz e Mesh Renderer
(`claude/vigorous-jones-3ce42f`), commit de merge 77807373.

APK Astra Dev `0.3.0-dev.150-conjunto-unity` (code 150), canal `dev.aether.editor.u07`,
no POCO F7. Cena: Vila Astra (cópia tirada antes, md5 `2e2e996841f1f0eb5af1572c68dec3fc`,
devolvida no fim com o mesmo md5).

## Resolução dos conflitos

- Bloco de uniformes do quadro: nuvens (G4b) em 3536..3599 e o segundo banco de
  luzes do PK-3 a partir de 3600 (`punctualLightsExtra[32]`); `DirtRoadFrameUniform`
  com 4112 bytes e `static_assert` dos três offsets.
- Menu Criar › Luzes com "Volume de nuvens" e "Iluminação da cena"; 99 receitas;
  51 capacidades da engine.
- Catálogo de ícones unido (dia e noite, auditoria de materiais, nuvens, fonte do
  sol, projetor de sombra, rendering layers); atlas regenerado com 380 ícones e os
  44 shaders regenerados a partir das fontes mescladas.

## Testes

- Host: `aether_tests` 1601/1601 (as 35 falhas da primeira rodada vinham de dois
  objetos com `#deps 0` deixados por um build interrompido, não do código).
- Gerenciada: 523 passaram, 0 falharam, 71 pulados; API C# gerada em dia.

## Capturas

| Arquivo | O que mostra |
|---|---|
| `01-vila-play.png` | Play com câmera fixa: céu, sombras e personagens normais no APK 150 |
| `02-criar-luzes.png` | Criar › Luzes com os seis itens e ícones das três branches |
| `03-volume-nuvens-inspector.png` | Volume de nuvens criado: Ambiente de prioridade 1 no Inspector |
| `04-vila-play-nuvens.png` | Play com o volume: nuvens no céu e luz atenuada pela sombra delas |

## Não validado aqui

- Mais de 8 luzes locais visíveis ao mesmo tempo (o banco extra): a Vila não chega lá.
- G2 (exposição/Volume), PK-7 (câmera) e clima ainda estão nas branches deles.
