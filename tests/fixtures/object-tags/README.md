# Aceite de tags — O1a

Este projeto mínimo contém Driver, Parent inativo e Child com `Alvo`. Copie
`TagProbe.cs` para a raiz do projeto, `scenes/` para as cenas e `tags.astra`
para `.astra/tags.astra`. Use projeto independente, sem pacote de recursos.

Compile/aplique o código no editor e entre em Play. O script deve registrar
`TAGS PASS`, após verificar a atribuição salva, comparação em objeto inativo,
busca global com ativação herdada, troca de tag em UTF-8, rejeição de nomes
desconhecidos e exclusão imediata de objetos com destruição pendente.
Stop deve recuperar Child com `Alvo` e Parent inativo.

Para validar autoria pelo toque, comece Child com `Untagged` e remova `Alvo`
do catálogo (atualizando a contagem no cabeçalho para 1). Selecione Child →
Objeto → Tag → Nova → Alvo → escolher Alvo. Desfaça/refaça, salve, encerre e
reabra o projeto antes de executar o script. Selecionar Parent e Child permite
repetir a atribuição múltipla; o script redefine Parent em Play.

Validação em 28/09/2026: fixture compilada pelo compilador C# do projeto no
host; cenários equivalentes executados no runtime nativo e no roteador de
toques do editor. No dispositivo 25053PC47G, projeto isolado O1Tags0928:
criação e atribuição pelo toque, edição múltipla, undo/redo, salvar/reabrir e
duas execuções com **TAGS PASS**, incluindo uma após reabertura com o APK final.
Stop restaurou Parent inativo e Child com Alvo. Evidências e limites em
[O1a tags](../../../docs/planos/O1A-TAGS-2026-09-28.md).
