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
| N03 | Rig genérico, poses, espelho e retargeting com repousos distintos | **Fechado em 11/10/2026** (commit seguinte a cd969f10): estúdio de animação em painéis, rig autorável com limites, IK de dois ossos com direção da dobra e limites, espelho de membro, pares por nome, biblioteca de poses como chave, fantasma, filtro da timeline e transferência de clipe entre personagens com repousos distintos. Host 60/60; aparelho (Astra Dev, Fox 27 juntas) com estúdio, rig, IK, espelho, fantasma e filtro. Limites declarados em docs/validacao/n03-rig-2026-10-10/README.md |
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
| N16 | Aceite integrado, docs e distribuição | Parcial; docs de desenvolvimento publicadas no commit aff741b, deploy READY. Distribuição e aceite final pendentes |

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

## Pausa solicitada pelo proprietário

Em 10/10/2026, após o commit `d7d8a331`, o proprietário pediu atualizar as docs e parar. Meta pausada e subagente interrompido. A baseline validada permanece em Git; alterações posteriores N03/N08 foram preservadas no working tree, sem nova validação integrada nem anúncio de disponibilidade.

Preparação N03: AERPOSE2 e revisão própria, tipos de recursos 16–18, guardas de ID, placement/reparent e autoridade de escrita; ainda faltam integração completa, autoria e aceite. N08: worker de bake e envelope tipado de lote preparados, sem publicação multirrecurso/SDK 13 ligados. Esses arquivos não pertencem à evidência 55/20/30/1 do commit validado. Antes de retomar builds, concluir a integração compartilhada e conferir os arquivos preservados. ADB liberado pelo proprietário em 10/10/2026 para validação contínua.

Docs públicas revisadas no commit AstraDocs `aff741b`: 1.396 HTML, 1.395 Markdown/JSON e 328.937 links sem erros. Revisão local em 390/655/1440 px sem overflow, imagens quebradas ou erros JavaScript. Nota nova em Atualizações conserva distribuição code28/Dev33 e distingue fonte, host e aceite Android. Push main concluído; deploy de produção READY e domínio associado confirmados pela Vercel.

O push não iniciou deploy automático observado; a publicação foi solicitada à
Vercel diretamente pelo commit exato `aff741bd1343bbe8dc7f3c22ea5319471dcd3061`,
sem enviar arquivos privados ou alterar configurações do projeto. Deploy
`dpl_G7PGuKVmb6hXShJxefhcGhseRXZ7` READY, associado a
`https://astraengine.com.br` e ao SHA exato acima.

Revisão pública concluída nas rotas de autoria e Atualizações em 390×900,
655×300 e 1440×900: seis respostas HTTP 200, sem overflow horizontal, imagens
quebradas ou erros JavaScript. Menu mobile abriu e fechou; contagens 55/55 e
20/20 e a nova nota no feed público foram verificadas. Relatório e capturas em
`docs/validacao/n01-n07-2026-10-10/host/docs-browser/`. O plano permanece
pausado e incompleto; essa publicação não constitui novo APK nem aceite Android.
