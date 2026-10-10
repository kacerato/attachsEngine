# Execução do plano Astra de 10/10/2026

Objetivo autorizado: implementar o plano inteiro, incluindo seus anexos e os
pacotes fornecidos. Este registro acompanha trabalho em andamento; não declara
encerramento dos 16 blocos nem paridade com engines de referência.

## Contratos da execução

- Base: `db8a26c7440fef2f0c214a0ce53669570b17010e`, branch
  `codex/gameplay-runtime`. Preservar alterações anteriores do proprietário.
- Editor mobile first, em landscape; autoria por controles reais da engine.
  Scripts de preparação e testes não substituem o caminho de criação no editor.
- Runtime, recursos, propriedades, persistência, lifecycle, Undo e API compartilham
  a mesma implementação. Falhas de conversão precisam de diagnóstico explícito.
- ADB, instalação e aceite físico aguardam a liberação do proprietário no final
  da implementação de todo o plano. Capturas host não substituem essa etapa.
- N13 decidido pelo proprietário nesta execução: servidor dedicado **e** jogador
  hospedando a sessão. A simulação permanece autoritativa no servidor em ambas
  as modalidades; cliente hospedador não recebe autoridade adicional implícita.
- Documentação pública no checkout AstraDocs, com rotas de desenvolvimento
  separadas do APK distribuído, nota nova em Atualizações e evidência de deploy.

## Ordem, dependências e estado

| Bloco | Resultado e dependência principal | Estado desta execução |
|---|---|---|
| N01 | Poses, auto-key, curvas, camadas, intervalos e cues | Parcial; autoria integrada 55/55 host aceita, incluindo poses, curvas, regiões e cues. SDK ABI 12 aceito contra nativo. Cenário Kyle completo e aceite Android pendentes |
| N02 | Propriedades tipadas e drivers; depende de bindings e preview restaurável | Parcial; GUI 30/30 e SDK ABI 12 contra DLL nativa 1/1, sem pulos. AECLIP 7, catálogo/bindings, compositor e preview compilados; busca, remapeamento, curvas e DAG possuem UI executável. Aceite vertical de propriedades/drivers passou com toque, prefab, reabertura, falhas atômicas e Play; capturas CPU inspecionadas. Aceite Android pendente; ver contrato N02 |
| N03 | Rig genérico, poses, espelho e retargeting com repousos distintos | Em implementação paralela; recurso, matemática, consumer e dois cenários passaram sintaxe estrita; execução e autoria/editor pendentes |
| N04 | Constraints, IK, full body, grounding e interação; depende de N03 | Pendente |
| N05 | Locomoção, apoio, root motion, aparência e câmera; depende de N03/N04 | Pendente |
| N06 | Bake e exportação com relatório de perdas; depende de N01–N05 | Pendente; preservar bake existente |
| N07 | Biblioteca dos pacotes, formatos e reimportação | Parcial; biblioteca 20/20 host aceitos, incluindo TIFF/FBX, takes, extração, reimportação e conversões Unity delimitadas. Corpus: 520 FBX estáticos e 150/151 deformáveis; dual quaternion recusado. Biblioteca integral e aparelho pendentes |
| N08 | SDK de autoria, transações multirrecurso e extensões | Parcial: ABI 12 tipada, catálogo e drivers aceitos contra nativo; transações multirrecurso, jobs e extensões pendentes |
| N09 | Sequências de cena multirrecurso | Pendente |
| N10 | UI componível, input, layout, texto, dados, canvas e acessibilidade | Pendente; seguir também T01–T40 |
| N11 | Modelador visual, topologia, UV e publicação | Pendente |
| N12 | Física, consultas, composição e regeneração | Pendente; preservar autoria física existente |
| N13 | Save, replay e rede autoritativa nas duas topologias | Decisão de produto registrada; implementação pendente |
| N14 | Render, atmosfera e todas as famílias do anexo | Pendente; capacidade condicional exige consumer externo real |
| N15 | Custo, escala, budgets e desempenho sustentado | Transversal; medições finais Release/dispositivo pendentes |
| N16 | Aceite integrado, docs e distribuição | Parcial; docs de desenvolvimento publicadas no commit d6eaec0, deploy READY e navegação responsiva verificados. Distribuição e aceite final pendentes |

O universo original está preservado em
`output/pdf/Astra-proximos-blocos-2026-10-10.md`: páginas 1–34, índice UMotion
de 75 páginas e mapa de 91 famílias. Os anexos são responsabilidades de cobertura,
não provas de implementação. Não se deve eliminar itens para encerrar um bloco.

## Evidência e documentação

- [Cobertura dos plugins](plugins-cobertura.md), inventário individual com hash em
  `plugins-itens.csv` e declarações em `plugins-declaracoes.csv`.
- [Decisões da interface N01](../../design/n01-2026-10-10/README.md).
- Logs e capturas deste ciclo em `docs/validacao/n01-n07-2026-10-10/`.
- Aceite final distingue: código revisado, build host, SDK gerenciado, recurso
  reaberto, pacote Android, instalação, runtime e exame visual no aparelho.

Uma linha em uma tabela, API exposta ou teste aprovado isoladamente não fecha
um bloco. O fechamento exige o cenário completo descrito no plano.
