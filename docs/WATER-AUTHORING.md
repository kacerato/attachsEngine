# Água autoral: superfícies, rios e física

## Decisão

A geometria, o documento e as consultas físicas usam recursos nativos da engine.
A referência KWS separa malha, espectro, fluxo, espuma, óptica e interação;
essa separação orienta a integração, sem introduzir dependências Unity no runtime.
Foram examinados WaterSystemScriptableData.cs, SplineScriptableData.cs e
KWS_SplineMesh.cs na cópia local do projeto de referência, além do quadro
https://trello.com/b/BUa7pLdp/water-system (snapshot em build/water-reference-board.json).
Não há declaração de paridade com todos os efeitos do KWS.

## Uso no Android

1. Abra uma cena e toque no **+ da Hierarchy**.
2. Escolha **Water surface**, **Open ocean**, **River route**, **Buoyant box** ou
   **Empty object**. Nenhum desses recursos cria instâncias na importação.
3. A superfície finita começa com 20 × 20 m. Transform define posição, altura
   e dimensões. O oceano usa a grade que acompanha a câmera.
4. **Route** permite selecionar, adicionar, remover e arrastar pontos. Cada
   ponto expõe X/Y/Z, largura, profundidade, correnteza, espuma e tensão da curva.
   A altura também pode ser editada numericamente. As setas inferiores percorrem
   todas as propriedades; o indicador mostra a página atual.
5. **Physics** ativa o volume e configura profundidade e corrente uniforme.
   No rio, a profundidade e o fluxo por ponto se somam à configuração aplicável.
6. **Effects** controla as camadas de ondas, espuma, ondulações e absorção.
   Zero desativa a contribuição correspondente. Correntes transportam o detalhe
   visual e os corpos; profundidade também limita a espessura óptica.
7. Uma **Buoyant box** já possui corpo dinâmico. Outras malhas podem receber
   física em **Props / Rigid body**, com massa, arrasto e dimensões do collider.
8. **Play** cria um mundo Jolt a partir do documento. Ao sair, as poses autorais
   são restauradas. Falhas interrompem a simulação e mostram mensagem no editor.
9. **Settings** expõe espectro e óptica globais: vento, fetch, swell, profundidade,
   espalhamento, mar cruzado, amplitude/choppiness por banda, espuma, IOR,
   turbidez, opacidade, absorção, rugosidade e nível. O layout aceita 1–4 cascatas,
   FFT 32–256, bandas logarítmicas, seed, domínio e orçamento de memória.

Criação, pontos, valores, duplicação e exclusão usam o histórico existente.
Salvar/reabrir preserva os recursos e componentes; a cena não depende de uma demo.

## Contratos e custos

- `WaterRoute`: 2–16 pontos, Hermite com tensão por ponto, 16 subdivisões por
  segmento e 8 faixas transversais. A malha tem até 2169 vértices por rio.
  Degeneração, números não finitos e limites inválidos são recusados.
- `WaterField`: recorte, altura, normal, profundidade e corrente usam a rota.
  A consulta inverte a transformação autoral para preservar a largura sob escala
  não uniforme. Superfícies quase verticais não são volumes de água válidos.
- `WaterWorld`: até 16 volumes, identidade do documento e filtros por layer.
  Sobreposição resolve prioridade, altura e ID, em ordem estável.
- `EditorWaterPlay`: até 128 corpos, passo fixo de 60 Hz, poses separadas do
  documento, massas Jolt reais, empuxo, arrasto, torque, correnteza, impacto e
  esteiras. Usa espelhos espectrais de até 128² e ripple field de 128²/128 m.
  O relógio publicado no renderer acompanha o passo da física.
- `AetherPhysics_ApplyWaterForcesV2`: limita volume deslocado e centro de empuxo
  entre a superfície e o fundo. A ABI anterior permanece como água sem fundo.
  O fundo do volume não cria terreno nem collider sólido automaticamente.
- Buffers de rios são substituídos na publicação do documento, após a fence.
  Durante Play, somente as poses dinâmicas são publicadas; não há tesselação nem
  reconstrução dos buffers de rios por frame.
- FFT estrutural prepara novos recursos antes de trocar descriptors. O orçamento
  é estimado e a substituição pode manter dois conjuntos temporariamente.
  O fallback analítico permanece disponível.

## Interface

O editor e o shell usam grafite, contraste neutro e seleção azul. A criação
possui popup próprio; o Inspector da água tem Surface / Route / Physics / Effects.
29 ícones vetoriais de água e ferramentas foram integrados ao atlas pelo gerador determinístico
`tools/generate-water-authoring-icons.py`, com fontes SVG preservadas.

A UI nativa é composta num passe de apresentação, após o processamento da cena
**e após a cópia do histórico temporal**. Assim ela não recebe bloom, tonemapping,
TAA ou escala dinâmica. Cores sRGB são convertidas somente quando o attachment
faz a codificação. Linhas projetadas são recortadas antes da interpolação GPU.

## Persistência e compatibilidade

Formato `AETHER_EDITOR 6`, leitura de versões 1–5 mantida. Metadados comuns
alimentam Inspector, validação, histórico e arquivo. As flags e propriedades de
água/corpo são copiadas em duplicação e restauração. Recursos gerados são anexados
à biblioteca AEMAP, preservando os IDs dos lotes anteriores.

## Limites explícitos

O espectro e a óptica principal são compartilhados pela cena; as contribuições
por superfície são independentes. O campo de ondulações é regional, centrado
no primeiro corpo na inicialização. Não é uma solução volumétrica de fluidos.

Não estão integrados nesta etapa: pintura/importação de flowmaps, spray volumétrico,
caustics, underwater, refração da cor da cena, malha arbitrária importada como
água e materiais de água reutilizáveis como assets independentes. As propriedades
estão disponíveis na metadata nativa; não se declara uma UI NoCode ou um binding
de script novos que não tenham sido implementados.

## Evidências

715 testes host aprovados, incluindo criação/paginação por toque, rio rotacionado/escalado, correnteza,
Jolt real, preservação da cena, fundo físico finito e recorte de linhas extremas.
Debug, Release e testes Java executados. Logs:

- `build/editor-host/water-routes-tests.log`
- `build/editor-host/water-routes-build.log`
- `build/water-routes-final-android.log`
- `build/water-routes-shader-*.log`

A versão intermediária foi instalada e capturada no aparelho. A captura
`build/editor-host/water-ui-sharp.png` mostra a composição em resolução de tela.
A compilação final foi instalada com `adb install -r`: Success. O Package Manager
registrou a atualização em 2026-09-08 20:39:25, no aparelho 25053PC47G.
SHA-256 do APK Debug: `F2223B7BE481AF99C3D41EBD16D5264ED4D92AB0114C92AA0422251F8BEA04AF`.
O teste final de Play no aparelho ficou com o usuário, conforme solicitado.
A aprovação host/build não equivale a benchmark GPU ou aceitação visual de todos os efeitos.
