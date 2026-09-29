# Prefabs — comparação e reversão seletiva

29/09/2026. Avanço parcial da etapa 7; não encerra o bloco ampliado.

## Capacidade implementada

Na instância, tocar a linha da fonte abre a comparação do objeto atual.
Cada diferença mostra valor local, valor da fonte e Reverter. Em tela compacta,
a rota ocupa temporariamente a área de trabalho; voltar conserva as larguras
dos painéis e a câmera. Em tela larga, permanece no Inspector.

- Nome, ativação, visibilidade, sombras, tag, camada e eixos de transformação.
  Posição e rotação da raiz são posicionamento da instância e ficam preservadas.
- Campos nativos refletidos e campos autorais C# por identidade. Remover uma
  atribuição C# que não existe na fonte devolve o controle ao default do script.
- Componentes adicionados localmente podem ser removidos; componentes removidos
  podem ser restaurados com a identidade original, preservando referências.
- Dados sem reflexão suficiente são apresentados explicitamente como componente
  completo. Reverter esse item substitui todos os seus dados. Arrays C# são um
  campo completo, sem equivalência por elemento declarada.
- Cada reversão passa por validação e uma transação de Undo/Redo. Cena alterada
  após a prévia, fonte alterada, fonte ausente, identidade ambígua ou referência
  ausente recusam a operação. Não há mutação parcial nem passo vazio no histórico.

A comparação é contra a **fonte atual**, não um merge de três versões. Ainda
não distingue visualmente alterações locais de alterações externas da fonte.
Mudanças estruturais dos componentes na fonte são recusadas até existir a
reconciliação com mapas persistentes de identidade. Instâncias antigas com
baselines diferentes da estrutura atual também exigem essa reconciliação.

## Correções necessárias encontradas na integração

`ASTRA_PREFAB 2` persiste a fronteira de alocação dos objetos. Um ID apagado não
é reutilizado após carregar e editar a fonte. A leitura da versão 1 continua
suportada; gravação usa versão 2. Leitores antigos não leem a versão 2. A fonte
v1 só permite recuperar a fronteira a partir dos objetos ainda existentes.

A validação de malhas agora consulta a existência do MeshRenderer antes de
obter acesso mutável: não acrescenta renderizadores a objetos vazios.

A gravação atômica usa caminhos UTF-8 e APIs wide no Windows. A prévia com o
recurso `Câmera de acompanhamento.prefab` reproduziu a falha anterior e passou
a criar e reabrir corretamente o recurso, sem remover os acentos.

## Referências e comparação

- [Unity 6000.0 — Override prefab instances](https://docs.unity.com/en-us/engine/6000.0/manual/working-with-gameobjects/prefabs/override/prefab-instance-overrides):
  separação entre alterações de propriedades, componentes e filhos; reversão
  individual e preservação da posição/rotação da raiz.
- [Godot 4.5 — PackedScene](https://docs.godotengine.org/en/4.5/classes/class_packedscene.html)
  e [código de SceneState](https://github.com/godotengine/godot/blob/4.5/scene/resources/packed_scene.cpp):
  estudar identidade, ownership e instância como recurso composto; a Astra usa
  seu grafo e serialização existentes, sem copiar classes da referência.
- As imagens da Unity foram examinadas no navegador em 29/09, com a versão
  6000.0 selecionada. [Comparação visual e funcional](../validacao/COMPARACAO-UNITY-ASTRA-PREFABS-2026-09-29.html).
  A pesquisa de vídeo não forneceu uma sessão assistida; não se declara evidência
  de workflow em vídeo.

## Validação

- Build host de `aether_tests` e `aether_ui_preview`: passou.
- Filtro `prefab_`: **9/9**, incluindo três cenários novos de identidade,
  reversão seletiva, referências, persistência, Undo/Redo e prévia obsoleta.
- Regressão `import_`: **51/51**; `duplic`: **10/10**. Filtros podem se sobrepor.
- UI rasterizada pelo executável real de preview: retrato 480×900, paisagem
  1200×700 e cena alterada após a comparação. Sem instâncias de UI descartadas
  ou glifos ausentes. A prévia usa criação, recurso e comparação reais da sessão;
  o fundo não contém renderização 3D e não é uma captura do Android.
- [Evidências host](../validacao/evidencias/p-prefab-overrides-20260929/).
- Android: `:app:assembleDebug` passou em 57 s. APK SHA-256
  `9D1F244541CD057E46428E8137952FCB73565D3B41A728562A18942CB7EC56B3`.
  O aparelho reconectou por ADB, mas estava adormecido e a captura retornou
  inteiramente preta. Instalação deste APK e aceite visual físico permanecem
  pendentes; foi solicitado desbloqueio. Build não equivale a aceite no aparelho.

## Continua pendente

Apply na fonte; propagação que preserve overrides; seleção múltipla de
diferenças e Apply/Revert All; objetos adicionados/removidos/reparentados;
ordem de componentes; marcas de alteração nas propriedades e na hierarquia;
edição contextual da fonte; aninhamento e variantes. A nova rota de comparação
não é a rota de edição da fonte da etapa 8. Não há nova família de componentes
entregue nem aumento da contagem de capacidades completas neste pacote.
