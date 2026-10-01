# F006 — Receitas com alvos declarados

Uma receita não pode perder silenciosamente o comportamento do objeto capturado. O formato v2 removia referências de cena e ignorava componentes sem codec portátil. V3 conserva o significado das referências e recusa a captura inteira quando não consegue conservar a composição.

## Contrato e ownership

`EditorComponentPresets` grava componentes nativos portáveis, valores, recursos persistentes, ordem autoral e endereços de referência. O arquivo não leva ObjectIds da cena de origem. Referências próprias usam o objeto de destino; alvos externos diferentes tornam-se entradas explícitas. Campos que apontavam ao mesmo objeto compartilham a entrada. Os nomes indicam tipo/ocorrência/campo, distinguindo, por exemplo, Timer 2 e Timer 3.

O resolvedor existente cria dependências reais no candidato. Componentes singleton existentes recebem valores; instâncias repetíveis são acrescentadas. A ordem dos componentes expressamente autorados é restaurada nos seus slots, conservando dependências automáticas e componentes não envolvidos. Referências são verificadas sobre a composição final; um Collider que aponta para o próprio Body criado na receita não é recusado por validar antes da criação. Recursos são resolvidos pelo caminho real do projeto. Erro ou entrada incompatível preserva documento e histórico.

Preparação e aplicação usam a mesma validação. Escolher um argumento só modifica o rascunho da rota. Aplicar publica uma transação em `EditorHistory`; Undo/Redo e save/reopen mantêm valores, ordem e referências. Os componentes aplicados são consumidos pelos sistemas existentes; não há RecipeComponent ou segundo scheduler em runtime.

## Compatibilidade e fronteira de composição

`ASTRA_COMPONENT_PRESETS_3` acrescenta entradas externas e bindings por PropertyId. Bibliotecas v1/v2 continuam abrindo. Tipos/versões desconhecidos são conservados na biblioteca, mas recusados na aplicação; escrita verifica os bytes originais para não sobrescrever alteração externa. Limites de arquivo, quantidade e endereços continuam explícitos.

Receitas são autoria de componentes nativos com codec portátil. Comportamentos C# e componentes desconhecidos não são descartados, copiados sem fonte ou tratados como preset suportado: captura recusa o conjunto e orienta usar o fluxo real de prefab para a composição completa. Metadados de ownership de prefab/importação pertencem à cena e são excluídos intencionalmente. Preset de componente único conserva referências já escolhidas no destino; receita conectada, mesmo com um único componente, exige preparação/remapeamento. Não se afirma equivalência integral ao Preset Unity nem portabilidade de projetos sem os recursos referenciados.

## Interface e referência

NÃO IREI SER SIMPLISTA NO DESIGN.

Biblioteca → preparar receita no objeto escolhido → escolher entradas com o seletor real → conferir composição → aplicar → editar componentes normalmente. Em paisagem pequena, preparação e seletor usam uma superfície temporária abaixo da toolbar, retornando à biblioteca/Inspector sem mudar geometria salva dos painéis ou câmera. O seletor conserva busca, modo avançado, compatibilidade e paginação existentes. As referências próprias aparecem no resumo da composição. Apply só ganha região de toque quando o candidato está válido.

A captura inicial 853×394 mostrou campos cortados. [Proposta gerada](../validacao/evidencias/families-recipes-20261001/concept-phone.png) foi confrontada com os controles reais: biblioteca, rename/delete, título/destino, status, campos de alvo, composição e transação existem no modelo. A implementação adapta o espaço temporário e linhas horizontais; não copia os cards da hipótese. Usa ícones existentes de objeto/referência do atlas real; não introduz um conceito de runtime que exija novo ícone. Capturas executáveis posteriores são a prova da implementação visual, separadas da hipótese.

Referências concretas: [Unity 6000.0 Presets](https://docs.unity3d.com/6000.0/Documentation/Manual/Presets.html), [API Preset 6000.0](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Presets.Preset.html) e [Godot 4.5 Resources](https://docs.godotengine.org/en/4.5/tutorials/scripting/resources.html). Princípios extraídos: configuração reutilizável de autoria, verificação do tipo de destino, recurso separado de instâncias e aplicação explícita. Bindings externos e aliases são a adaptação Astra para composição portátil, não uma alegação sobre semântica equivalente nessas engines.

## Aceite

Capturar Timer próprio + Timer externo + Follow compartilhando alvo + Camera → fechar/reabrir biblioteca → recusar alvo não preenchido sem mutação → aplicar ao novo objeto → conferir aliases e ordem → Undo/Redo → salvar/reabrir → executar o scheduler real e observar ativação/desativação. Segundo cenário: Body+Collider com ownership próprio só é validado após a composição; alvo ausente e preview obsoleto recusam publicação. Terceiro: fluxo real de ponteiro 853×394, seletor/paginação, rascunho sem mutação, Apply e Undo.

[Evidências](../validacao/evidencias/families-recipes-20261001/README.md) separam cenários host, captura executável, build/APK e limite físico. Sem instalação/execução Android nova nesta rodada.
