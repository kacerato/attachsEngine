# Proposta original N07 — catálogo e viewer independente

Registro histórico da decisão, não estado atual de suporte. Catálogo, rota mobile
e viewer independente foram implementados depois desta proposta; o aceite host
está nos documentos `N07-biblioteca-indice.md` e `N07-materiais-biblioteca.md`.
Android/Vulkan físico continua pendente. Os parágrafos de proposta abaixo explicam
a arquitetura escolhida e não substituem o ledger de implementação e validação.

## Problema concreto

O inventário ampliado tem 2.816 entradas, incluindo POLYGON; hoje o autor navega arquivos, sem uma
visão unificada da origem, versão, conversão e dependências por pacote. A prévia
contextual usa um objeto real da cena. O pacote posterior acrescenta geometria
independente para material sem usos; o aceite visual GPU ainda falta.

NÃO IREI SER SIMPLISTA NO DESIGN.

A proposta é tornar Biblioteca um destino da superfície contextual existente:
busca, breadcrumb pacote → família → item, filtro por estado e detalhe sob
seleção. O viewport permanece dominante; abrir viewer substitui temporariamente
a superfície pelo recurso e volta ao mesmo item/filtro. Lista densa para texto,
grade apenas para materiais/modelos com miniatura real disponível. A origem e o
diagnóstico não são escondidos atrás de uma imagem que sugira suporte.

## Catálogo persistente e consumidor

Um índice tipado de biblioteca guarda pacote/origem/versão, identidade do item,
hash da fonte, kind, GUID nativo quando convertido, diagnóstico e receita usada.
O AssetRegistry continua dono das identidades nativas/dependências. O índice
não duplica material, textura, prefab ou animação, nem infere suporte pela extensão.
Mudança/remoção no registry invalida o estado disponível da entrada.

Workflow mínimo: adicionar pacote de diretório → inventariar com cancelamento e
progresso → ver suportados/recusados/pendentes → converter pelos backends reais →
abrir/reusar recurso → salvar/reabrir mantendo origem e filtros. O job não pode
expor mutações parciais como recursos disponíveis. Erros mantêm item e fonte;
miniatura somente após consumidor publicar com sucesso. Renomeação precisa
reconciliar GUID Unity, não apenas nome/caminho. CSV interno atual não substitui
esse formato e não deve ser embarcado como promessa de suporte.

Arquivos a ampliar: `editor_filesystem.h/.cpp`, `editor_screen.*`,
`editor_session.*`, tarefas de importação/cancelamento e AssetRegistry; recurso
novo `resources/library_index.*` somente para os metadados persistentes que o
registro atual não possui. Autoridade de estado vem dos conversores/registry.

## Viewer independente, sem alterar a cena

A referência concreta é Godot 4.5
[MaterialEditor](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/material_editor_plugin.cpp):
mundo separado, câmera, luzes e esfera/caixa/plano reais. Princípio extraído:
inspecionar um recurso não exige salvá-lo em uma entidade do jogo.

`MaterialPreviewResources` existe na Astra, mas é showcase de inicialização com
esfera 192×96 e cinco texturas fixas do APK; não é preview dinâmico de recurso.
Reutilizar sua malha sem resolver materiais/texturas/UV não conclui o viewer.

Ampliar o caminho de preview independente existente para uma variante material
standalone: geometria PBR em `MapVertexStride`, buffers GPU pertencentes à prévia,
snapshot imutável dos parâmetros efetivos, bindings resolvidos da biblioteca de
texturas e transformações UV próprias. O mesmo pass PBR deve consumir tudo que
o Inspector expõe. Não adicionar entidades temporárias ao documento nem injetar
desenhos/instâncias no pacote principal. A câmera de prévia possui órbita/zoom
próprios; um dedo gira, dois aproximam/deslocam conforme ergonomia existente.

Controles iniciais com consumidor real: esfera/caixa/plano, órbita/zoom, ambiente
da cena ou ambiente de estúdio explicitamente carregado, exposição da prévia.
Valores do material continuam editados pelo Inspector compartilhado existente.
Ao trocar shader/tipo incompatível, mostrar diagnóstico; sem placeholder silencioso.

Arquivos: `renderer/render_view.h`, `editor_camera_preview.h`, helper de material
preview, `instanced_camera_preview.inl`, `instanced_renderer.h`, shader/descriptor
UV quando necessário, shell Android e rota contextual de UI. Reaproveitar leases,
orçamento e render targets existentes; fechar/trocar projeto/perder contexto deve
cancelar lease e liberar recursos exclusivos. Texturas compartilhadas permanecem
com ownership da biblioteca.

Antes de implementar, verificar o layout de descriptors e instance/UV buffers:
o shader atual indexa UVs por drawIndex principal, portanto a variante standalone
precisa endereço independente válido, não índice fictício. Pipeline, vertex
stride e push constants devem manter o contrato PBR real.

## Cenário de aceite

Adicionar pacote → converter Standard com base/normal/emission/occlusion e UV →
abrir material sem uso → trocar formas e ambiente → editar fator e UV → observar
resultado → criar objeto e vincular → comparar valor efetivo com preview →
desfazer/refazer conversão/edição → reabrir projeto e preservar identidade/origem.
Hash da fonte, diagnóstico de dependência removida e conflito local precisam
continuar acessíveis. Validar UI host 655×300 e 853×394, compilação Vulkan Android,
captura GPU e aceite físico somente quando o proprietário liberar ADB.
