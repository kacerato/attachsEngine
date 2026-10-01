# Componentes, Inspeção e lightmap — entrega de UX

NÃO IREI SER SIMPLISTA NO DESIGN.

Esta rodada amplia a interface executável já existente. Usa a skill `avoid-ai-design` no perfil `inside-design-system`: conserva tokens e identidade Astra, melhora descoberta e navegação sem introduzir outra linguagem visual. A geração conceitual da rodada anterior permanece hipótese, não evidência. O editor de cena continua horizontal.

## Caminhos e comportamentos

- Hierarquia → Componentes → Objeto: acesso explícito a Nome, Visível, Sombra, Layer, Tag e ao inventário Static. Antes os campos do objeto estavam em um cartão de outra página da Inspeção.
- Componentes → instância → Inspeção: as setas no cabeçalho navegam entre Objeto, Transform e componentes reais do mesmo alvo. Identidade usa `instanceId`; alvo travado e componentes comuns da seleção múltipla são respeitados. Alterar a rota limpa menus, pesquisa e paginação do alvo anterior. Os campos continuam usando seus contratos de Undo e persistência.
- Add Component → família → tipo → composição → adicionar: no telefone horizontal com largura suficiente, o trilho mostra nomes das famílias. A lista mantém busca e dependências reais.
- MeshRenderer → Lightmap → slot: enum por slot, textura e números refletidos pelo schema. O seletor usa o registro do projeto, GUID estável e o caminho de edição real do componente. Material permanece no editor com seu alcance próprio.
- Objeto → Static: sete caixas de referência Unity visíveis e desabilitadas, com motivo. Não gravam flags nem simulam consumidores. Navigation Static e Off Mesh Link Generation são marcadas como legado da Unity.

O caminho de enums por slot agora é genérico (`ComponentSlotEnumBase`, endereço componente/propriedade/slot), usa validação `setComponentSlotProperty` e publica via histórico. Enum de duas opções tem checkbox; demais opções são percorridas em ordem. Valores mistos usam o endereço editorial `instance/property@slot`, em acordo com a sessão. Os presets e o reset usam `FieldKind::SlotEnum`.

## Limites da superfície Lightmap

RGB linear de irradiância indireta, UV1 e backend bindless são contratos da cadeia externa. A superfície registra que não há bake nem unwrap nesta entrega. O diagnóstico verifica as malhas dos slots através de `EditorMapScene::lightmapUvStatus` e `lightmapUvDiagnostic`: malha ausente, coordenadas inválidas, triângulo degenerado, sobreposição de área ou orçamento de análise excedido. A análise CPU é limitada a 65.536 triângulos e um milhão de pares; exceder orçamento é indisponibilidade explícita. Isso não prova qualidade luminosa nem desempenho Android. O ícone novo `lighting/lightmap` representa luz recebida em ilhas UV; SVG, raster e atlas são publicados pelo pipeline existente.

## Referências verificadas nesta rodada

- [Unity 6000.0 — Use components](https://docs.unity3d.com/6000.0/Documentation/Manual/UsingComponents.html): Add Component, pesquisa, famílias, dependências e propriedades. Princípio aplicado: composição e criação permanecem acessíveis a partir do mesmo alvo.
- [Unity 6000.0 — Renderer.lightmapScaleOffset](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Renderer-lightmapScaleOffset.html): escala e deslocamento são dois pares UV no mesmo mapeamento de atlas. A interface da Astra reúne U/V por significado, conservando setters refletidos por canal. A Astra persiste esses valores no authoring; não reproduz a regra Unity de serialização somente no build do player.
- [Unity 6000.0 — Inspector options](https://docs.unity3d.com/6000.0/Documentation/Manual/InspectorOptions.html) e [Manage components](https://docs.unity3d.com/6000.0/Documentation/Manual/InspectorManageComponents.html): Inspector contextual, edição de propriedades e administração de componentes. A navegação entre componentes adapta a pilha desktop ao espaço horizontal do telefone.
- [UnityCsReference, ramo 6000.0 — GameObjectInspector.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Inspector/GameObjectInspector.cs): estado ativo e Static pertencem ao objeto; o dropdown usa propriedades serializadas dos alvos. Não há motivo para modelar Static como um componente fictício.
- [UnityCsReference, ramo 6000.0 — StaticEditorFlags.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/StaticEditorFlags.cs) e [Manual Static Objects](https://docs.unity3d.com/6000.0/Documentation/Manual/StaticObjects.html): consumidores distintos para GI, oclusão, batching e reflexão; membros de navegação legados. O inventário deixa essas diferenças explícitas.
- [Creating And Editing Prefabs in Unity — Gregory Osborne](https://www.youtube.com/watch?v=cbjRdGzgjVc&t=540s): nesta rodada a transcrição automática foi exportada e lida nos trechos 8:23–9:24; o vídeo foi reproduzido e pausado em **9:00** no navegador. O Inspector conserva um registro do Capsule Collider removido, com ações relacionadas à fonte, e o Add Component permanece abaixo da composição. Isso informa a distinção entre componente presente, dado preservado e alteração estrutural. Não foi verificado o vídeo inteiro nem identificada precisamente a versão do editor no quadro.

## Evidência de entrega

Prévia host posterior ao build consolidado: cinco estados em **853×394**, mais Lightmap e Static em **1200×700**. Todos produziram zero instâncias descartadas e zero fontes ausentes. O recorte de linhas de Componentes é intencional, com scroll; o viewport conserva 406×318 no telefone e 576×624 na prévia ampla.

A primeira captura de Lightmap revelou uma falha real: propriedades sem grupo de material/geometria entravam nessa rota e produziam 19 páginas. A correção limita a rota ao grupo Lightmap e transforma a busca em ação sob demanda. Uma versão intermediária ainda exigia quatro páginas para sete campos; o refinamento final reduz para duas páginas por slot no telefone. Habilitação, textura e escala UV aparecem na primeira página; deslocamento UV e intensidade na segunda. Em 1200×700 os sete valores aparecem juntos. O estado sem UV1 aparece explicitamente. Isso verifica a legibilidade e o fluxo da interface, não o resultado luminoso no aparelho.

Refinamento posterior: habilitação e binding da mesma textura passam a uma linha composta; Escala UV e Deslocamento UV passam a pares U/V. Os sete valores autorais continuam existindo, com os mesmos endereços de widget, mas ocupam quatro linhas por slot. A busca por propriedade continua expondo os canais individualmente. Cada eixo conserva teclado numérico, mixed value e seu reset; o checkbox conserva seu reset de enum independente do picker. Não foi reduzida a altura de 40 pixels das linhas editáveis para forçar encaixe. O Inspector focado existente foi avaliado: sua janela tem largura limitada a 300 e altura derivada do mesmo viewport, sem ganho suficiente para justificar trocar de rota neste pacote. As capturas executáveis posteriores confirmam as páginas 1/2 e 2/2, pares legíveis e ausência de sobreposição entre caminho da textura e lupa. As duas páginas do telefone registram zero recortes, fontes ausentes ou instâncias descartadas.

Static mostra duas referências por página no telefone, mantendo motivo e retorno acessíveis; em 1200×700 mostra as sete caixas juntas, incluindo os dois membros legados. Componentes mantém a composição rolável e Add Component fixo; Inspeção continua mostrando Alvo e Deslocamento juntos. O catálogo mostra nomes de família em horizontal e conserva a prévia do tipo escolhido.

![Lightmap no telefone — executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/component-surface-lightmap-final.png)
![Lightmap no telefone, página 2 — executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/component-surface-lightmap-page2-final.png)
![Lightmap amplo — executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/component-surface-lightmap-wide-final.png)
![Static — executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/component-surface-static-wide-final.png)
![Catálogo — executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/component-surface-catalog-final.png)

Arquivos adicionais no mesmo diretório: `component-surface-final.png`, `component-surface-inspection-final.png` e `component-surface-static-final.png`. São capturas de código executável, não imagens conceituais. O ícone novo foi gerado pelo pipeline SVG/raster e empacotado no atlas de 201 entradas. `git diff --check` não encontrou erro de whitespace nos arquivos deste pacote. O agente principal consolidou build host e Android e informou que o teste integrado Lightmap passou (1/1), incluindo persistência/reabertura, Undo, reset do checkbox, edição de V independente de U e picker real para limpar/reselecionar textura. Nenhum comando ADB foi executado por este agente; build Android não é validação visual no aparelho. A superfície de referência não conta como sete novas capacidades implementadas.
