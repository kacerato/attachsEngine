# Documento sem pacote de demonstração

Data: 2026-09-09. Marco M1 da refundação; validação em
`docs/REFUNDACAO-ASTRA.md`. Escopo: inicializar, persistir e retomar autoria sem
geometria importada. Não entrega M2, M3 nem Play genérico.

## Problema comprovado

`AstraShellActivity` iniciava Empty com `aether.map_preview`. O renderer
carregava DirtRoadResources antes de `EditorSession`. A carga/salvamento da
autoria estava condicionada a `mapDraws().empty()==false`. Não instanciar draws
na hierarquia escondia essa dependência, mas não a removia.

## Decisão

Novos descritores recebem `resourceSource: independent`. `ProjectSceneSource`
resolve a fonte pelo descritor ou fingerprint do arquivo nativo. Não usa nome
do projeto. Arquivo independente usa fingerprint zero; fingerprints anteriores
mantêm seus pacotes. Sem descritor novo e sem arquivo público, preservar o
caminho legado para que a migração do arquivo privado ainda possa ocorrer.
Cabeçalho inválido gera diagnóstico e não abre uma demonstração como fallback.

O shell transmite `aether.empty_workspace` ao renderer e inicializa o documento
mesmo com zero recursos. A API gráfica aceita apresentação de cena vazia:
clear, profundidade e overlay existentes, zero draws de geometria e nenhuma
extração managed. O asset manager só fornece atlas da UI nesse caminho.
O pipeline mantém um buffer mínimo de capacidade um exigido pela configuração
Vulkan atual; não é uma entidade oculta. A retirada desse buffer e pipeline
ociosos poderá acompanhar a extração genérica M3, sem criar outro renderer agora.

## Ownership, threads e erros

Sessão e documento pertencem ao shell. Inicialização GPU continua no worker
existente; `future.get` publica o resultado antes de usar os atlas. Pausa,
destruição/recriação da surface e retomada reutilizam o lifecycle existente.
Salvamento é independente da presença de draws. A sessão preserva seu documento
durante reconstrução GPU; reinício do processo relê o arquivo com namespace zero.
Falhas de arquivo não autorizam apagar o original. Mudança de projeto/fonte por
Intent numa Activity viva é recusada explicitamente até encerrar a cena atual.

## Alternativas

- Continuar importando um mapa e ocultar seus draws: rejeitado por violar M1.
- Gerar outro pacote obrigatório de demonstração: rejeitado; troca a dependência.
- Substituir o renderer por Godot: rejeitado; fora da autorização do plano.
- Refazer toda a RHI para a tela vazia: desnecessário para esse contrato.

## Limites e aceitação

Os arquivos antigos não são migrados automaticamente para zero: podem conter
referências à biblioteca original. Nenhum projeto do usuário é apagado.
Água ainda existe nos tipos/headers e no formato legado: extração em M2 continua
obrigatória. Não confundir ausência de carga/simulação com remoção do pacote do
build. GLB, materiais independentes e mesh authoring pertencem a M3/M5.

Testes: projeto novo, arquivo zero, arquivo legado, migração privada, cabeçalho
malformado e excessivo; roundtrip nativo de grupo sem biblioteca, publicação de
zero draws e preservação após fingerprint incompatível. Prova Android exigida:
criar pela UI, salvar, Home/retomar, reiniciar processo/reabrir, observar zero
recursos/fingerprint zero no log e comparar o APK instalado com o build.

Referência funcional consultada: [workspace 3D da Godot](https://docs.godotengine.org/en/stable/tutorials/3d/introduction_to_3d.html),
que distingue preview de ambiente/luz dos objetos da cena. Nenhuma API da Godot
foi presumida disponível na Astra ou introduzida nesta alteração.
