# Perfis de entrada do jogador — ABI26

## Referência e separação

Unity Input System **1.11.2**, [Input Bindings: overrides, saving/loading and restoring](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html#saving-and-loading-rebinds), distingue controles autorados e overrides do jogador. A Astra aplica esse princípio ao mapa existente, sem copiar paths genéricos: os vínculos permanecem tipados e passam pela validação real das fontes.

## Capacidade implementada

`InputService` mantém mapa autorado e mapa efetivo. `overrideBinding`, restauração individual/global, exportação e importação atuam no mapa consumido por gameplay, não no documento. Rebind solta estado anterior; foco e contextos permanecem intactos. Uma nova sessão de Play começa com autoria até carregar explicitamente um perfil.

Perfil `ASTRA_INPUT_PROFILE 1` inclui o contrato autorado e os valores efetivos. Importação compara o contrato completo: identidade, ordem e quantidade de ações/vínculos, papéis, contexto, processamento e fontes de origem. Somente valores dos vínculos podem diferir. Mudanças incompatíveis de projeto, registros inválidos, truncamento, lixo final ou mais de 256 KiB recusam a troca inteira. Não há aplicação silenciosa ao índice errado. Migração de perfis entre mapas alterados ainda não existe.

ABI26 acrescenta `ScriptInputBinding` de 24 bytes, `inputBindingCommand` e `inputProfile`, com callbacks obrigatórios e offsets/size conferidos. C# `InputBindingValue` expõe fonte, código, código negativo, eixo de saída, escala e inversão. `Input.GetBinding`, `ApplyBindingOverride`, `RemoveBindingOverride`, `RemoveAllBindingOverrides`, `ExportProfile` e `ImportProfile` chegam ao consumidor nativo. Recusas são explícitas; não são APIs decorativas.

`Input.SaveProfile(path)` grava arquivo temporário ao lado do destino e substitui-o, limpando temporários quando falha. `LoadProfile(path)` limita leitura e só troca o mapa após validar. O jogo escolhe seu diretório de armazenamento do jogador. O arquivo de cena não é usado como perfil, e a engine não inventa um diretório universal de saves nesta fatia. Erros de filesystem são exceções; arquivo ausente, oversized ou perfil incompatível retorna falso. Não se afirma durabilidade contra queda de energia.

## Workflow e limites

O mapa do editor continua sendo autoria. O jogo consulta o vínculo efetivo para apresentar seu menu, aplica uma escolha e salva seu perfil; ao iniciar, carrega-o explicitamente. Esta fatia entrega API e persistência, não uma UI de jogo completa nem captura interativa do jogador. A captura existente no editor não foi anunciada como menu de rebind em gameplay. Captura interativa, migração e perfis de múltiplos jogadores continuam pendentes.

Nenhum componente, receita ou ícone novo é necessário: 32 schemas, 31 fachadas, 226 ícones; cena16, prefab3, Timer3. ABI25 anterior é incompatível com o SDK ABI26 e não deve ser misturada ao novo APK.

## Validação

Override → entrada real consumida → exportar → restaurar → importar → entrada restaurada → novo Play com defaults; autoria não muda. Corromper perfil e mudar autoria deve recusar sem efeitos parciais. Arquivo: gravar/substituir, falhar exportação, manter anterior, eliminar temporários e limitar leitura.

[Evidências](../validacao/evidencias/input-profiles-abi26-20261001/README.md) incluem testes nativos, C# e fixture independente `InputProfile-20261001`. No Android, `InputProfileProbe` deve produzir PROFILE PASS e, após tecla62, PROFILE INPUT. O caminho fixo pertence somente à fixture isolada de aceite, não à implementação de engine. Não existe evidência desses logs enquanto o aparelho estiver bloqueado.
