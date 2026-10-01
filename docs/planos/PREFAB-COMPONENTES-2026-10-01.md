# Prefabs — reconciliação de componentes

Estado: bloco de componentes implementado e validado em host/APK. Este bloco amplia o Apply/Revert existente, sem criar um segundo formato de prefab nem novos tipos no Add. Não encerra F005: alterações da árvore de objetos, nested prefabs e variantes ainda exigem implementação própria.

## Contrato

Uma adição, remoção ou reordenação autorada de componentes é um endereço de override. O componente conserva sua identidade e referências internas remapeadas. Publicar seleciona endereços locais e prepara fonte, registro e instâncias carregadas antes de alterar qualquer arquivo. Os mesmos contratos `applyPrefabOverrides`, `revertPrefabOverrides` e `refreshPrefabInstance` são usados pela UI executável.

A comparação usa base salva B, instância I e fonte S. Adição ou remoção participa da mesma reconciliação de três vias que valores. Se a fonte remove um componente modificado localmente, a instância mantém o componente e um conflito persistente. Se dois lados criam o mesmo ID com conteúdo diferente, a decisão se refere ao componente completo; nenhum campo do outro lado é recebido como se fosse independente. Substituir fonte exige escolha explícita. Receber/Reverter usa os dados reais da fonte.

Ordem é um endereço próprio: aplicar a ordem dos IDs compartilhados mantém os slots de adições locais e dos metadados. Adições/remoções são reconciliadas antes da ordem. Metadados de vínculo podem ser movidos a uma identidade livre quando uma adição externa da fonte reutiliza seu ID; o vínculo não é substituído pelo componente novo. Referências incompatíveis bloqueiam a operação.

O candidato completo deve satisfazer requirements/conflicts dos schemas quando sua composição muda. Uma adição de Follow sem Câmera é recusada; selecionar ambos permite publicar a cadeia real. Nenhuma dependência padrão é fabricada silenciosamente. Uma remoção não pode invalidar uma referência tipada, inclusive de um objeto fora do prefab. A validação ocorre na operação de autoria, não no loop de renderização.

Undo/Redo reutiliza a transação de recurso existente: fonte e registro conservam bytes/identidade, e os valores e bases de todas as instâncias carregadas são restaurados juntos. Edição concorrente do arquivo ou do documento continua recusada antes da mutação. Cenas fechadas não são percorridas automaticamente.

## Referências

- [Unity 6000.0 — Instance overrides](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html): presença de componente é override, e dados locais têm precedência sobre defaults da fonte. A posição/rotação da raiz continuam colocação da instância na Astra.
- [Unity 6000.0 — ApplyAddedComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/PrefabUtility.ApplyAddedComponent.html): uma adição precisa publicar sua cadeia válida de dependências e referências.
- [Godot 4.5 — PackedScene](https://docs.godotengine.org/en/4.5/classes/class_packedscene.html): o recurso serializado e a instância são estados separados. A Astra conserva seu Prefab/SceneGraph/PrefabLink e não executa SceneTree Godot.
- As referências de arquitetura e workflow existentes constam de [Apply seletivo e propagação](PREFAB-APPLY-PROPAGACAO-2026-09-30.md). A limitação de componentes desse recorte de setembro é ampliada por esta revisão; a limitação de hierarquia/nested/variantes permanece.

## Cenários de aceite

`tests/native/test_prefab_structure.cpp` usa documentos, arquivos, registros, histórico e consumidores reais. O alvo dedicado também inclui os cenários anteriores de prefab, sem executar a suíte inteira da engine.

1. Adicionar Timer com alvo interno e reordenar componentes → Apply em duas instâncias → remapear alvos → Undo/Redo exatos → reabrir arquivo de cena → Timer desativa cada alvo no GameWorld real.
2. Remover componente enquanto outra instância altera seus dados → preservar conflito → salvar/reabrir → receber explicitamente a remoção.
3. Duas adições com o mesmo ID e valores diferentes → preservar local → recusar substituição sem escolha → receber componente completo.
4. Recusar dependência ausente; publicar a cadeia Câmera/Follow; recusar remoção que quebraria referência tipada externa, sem modificar fonte/instâncias.
5. Adição externa colide com ID de metadados → receber fonte conservando ownership → comparação convergente → Undo restaura o ID anterior.

Build, resultados e capturas estão em `docs/validacao/evidencias/families-prefab-20261001/`: 24/24 cenários direcionados, captura executável, Android final compilado e manifesto de bytes/hashes do APK ABI34. O cenário de referências também recusa Undo que removeria um componente agora referenciado fora do prefab. Nenhum aceite físico Android foi obtido nesta revisão.
