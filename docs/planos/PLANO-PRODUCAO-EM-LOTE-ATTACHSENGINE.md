# Produção em lote no attachsEngine

Proposta revisada em 2 de outubro de 2026. Repositório confirmado com acesso autenticado: `kacerato/attachsEngine`, privado, `main` em `97ecf56f17fe39e7d2fd527719b7fbce2528cd18`. A análise inicial usa arquivos desse commit, sem tomar o plano local Astra Forge como objetivo. O avanço implementado está delimitado abaixo; as demais entregas continuam propostas.

## Avanço implementado em 2 de outubro

A primeira parte da Entrega 1 está aplicada ao catálogo real, na branch local `codex/gameplay-runtime`:

| Parte | Evidência atual |
|---|---|
| Registro em lote | 45 tipos existentes em 10 tabelas de famílias gerados de contratos JSON |
| Propriedades tipadas | 352 declarações em 60 tabelas; 704 lambdas de acesso geradas |
| Dados e defaults | 189 campos nativos de 26 tipos gerados dos mesmos contratos |
| API C# | Exportação existente conectada ao comando único; 44 fachadas conferidas |
| Consistência | Build rejeita saída ausente/desatualizada; job de CI adicionado |
| Preservação | Snapshot compilado de 45 schemas/548 bindings idêntico antes/depois |
| Efetividade | Uma edição de contrato altera default e limite, compila e passa escrita, persistência e rejeição de valor inválido |
| Validação local | 6 testes do gerador e 16 cenários existentes de runtime passaram no host |

Uso: `python tools/generate-component-contracts.py --sync-api --build-dir build/editor-host` (build host previamente configurado). O exportador compila separadamente do renderer/editor/Jolt.

Persistência especializada, métodos/eventos, enums, referências, coleções e setters com efeitos particulares ainda possuem código manual. Não foram adicionados novos componentes ou backends nesta entrega. Não há medição de multiplicador de velocidade nem validação Android desta mudança. A próxima expansão deve atacar essas partes repetidas restantes, preservando os consumidores reais.

[Evidências e limites](../validacao/evidencias/component-codegen-20261002/README.md) · [Contrato e comandos](../../tools/component_contracts/README.md).

## A mudança de método

**Incremento de comportamento real:** [transporte de áudio ABI37](AUDIO-TRANSPORTE-ABI37-2026-10-02.md) conecta Play/Pause/Stop/Seek/Resume ao miniaudio, corrige replay após EOF, diagnóstico da última fonte desativada e limpeza persistente de GUID. F058/F059 continuam parciais: esse incremento não encerra streaming, prioridade/spatial blend ou mixer de efeitos. A automação anterior permanece disponível, mas contagens de propriedades não são usadas como evidência de conclusão de backend.

Incremento seguinte implementado: mais 22 propriedades Enum em 14 tabelas (374 declarações/74 tabelas no total). A geração preserva tipos fortes e reutiliza opções e consumidores reais. A comparação compilada passou também pelas 530 opções enumeradas instanciadas do catálogo, com snapshot idêntico antes/depois. [Evidências deste incremento](../validacao/evidencias/component-codegen-enums-20261002/README.md). Métodos/eventos continuam pendentes: a inspeção confirmou que exigem trabalho na ABI/runtime, além dos descritores de propriedades.

O pedido é aumentar radicalmente a produção e receber o conjunto funcionando. Uma lista de bibliotecas seguida de implementação artesanal, componente por componente, mantém o problema. A unidade de trabalho deve passar a ser **um módulo de comportamento completo e seu catálogo gerado em lote**.

O método proposto combina três coisas: gerar o código repetitivo antes dos descritores atuais, incorporar implementações reais de subsistemas e concentrar alterações de editor/build em entregas grandes. A ferramenta só tem valor se vier aplicada a um lote real; não deve virar mais um projeto de infraestrutura separado.

## Gargalo observado no código remoto

`native/scene/physics_body.h` e `native/scene/audio.h` ainda contêm campos, leitura/escrita, arrays de propriedades, callbacks, enums e metadados declarados manualmente. Já existem macros que reduzem parte da repetição; já existe geração C# em `component_api_csharp.h`. Portanto, simplesmente propor um gerador de C# ou macros novas não muda suficientemente o processo.

A intervenção é subir um nível: uma declaração compacta de família passa a produzir essas partes repetidas. A engine continua consumindo `ComponentType` e `ComponentSchema`; o gerador não introduz um segundo sistema de componentes em runtime.

## Entrega 1 Automação aplicada ao catálogo existente

Criar um gerador de código executado no desenvolvimento e aplicá-lo em lote aos contratos existentes. Ele recebe declarações da família, tipos nativos e vínculos com funções reais. Produz código C++ tipado compatível com a engine, em vez de interpretar mapas genéricos a cada frame.

| Entrada única | Saída gerada |
|---|---|
| Identidade, versão, campos, defaults e domínio | Dados simples, descritores e validação estrutural |
| Política de persistência e layout do formato | Leitura/escrita padrão, mantendo leitores e migrações existentes |
| Requisitos, incompatibilidades e criação | Registro da família e catálogo do Add |
| Propriedades, referências, unidades e grupos | Integração com os controles comuns já existentes |
| Métodos, argumentos, retornos e eventos | Fachadas e encaminhamento para funções implementadas |
| Binding com setter ou sincronizador nativo | Chamadas tipadas, conversões explícitas e invalidação declarada |
| Referência de documentação e ajuda | Documentação da API derivada da implementação |

O formato concreto da declaração deve ser escolhido pela menor mudança sobre os descritores atuais. Não criar uma linguagem complexa nem um compilador C++ próprio. Clang fornece leitura estrutural do C++ e acesso à configuração de compilação; usar isso quando for necessário extrair ou conferir símbolos. A geração por templates sobre uma declaração explícita pode ser suficiente para a primeira aplicação. [Clang LibTooling](https://clang.llvm.org/docs/LibTooling.html).

SWIG e CppSharp são alternativas prontas para gerar bindings, mas não substituem o runtime da engine. Como o repo já tem ABI e fachada próprias, acrescentá-los só faz sentido se reduzirem trabalho mensurável e preservarem handles e ownership. Não adotar ambos nem expor ponteiros de terceiros diretamente ao gameplay. [SWIG C# 4.3](https://www.swig.org/Doc4.3/CSharp.html), [CppSharp](https://github.com/mono/CppSharp).

Para dados novos e simples, gerar a estrutura inteira. Para classes existentes com comportamento e formatos específicos, gerar somente descritores e bindings ao redor delas. Não reordenar silenciosamente campos dos arquivos antigos nem remover leitores de versões anteriores.

**Resultado esperado da entrega:** aplicação ao conjunto de tipos existentes, geração reproduzível e nenhuma regressão de formato/API verificada nos cenários relevantes. Não encerrar com um gerador vazio e dois componentes demonstrativos. Exceções reais continuam manuais e são apontadas no mesmo lote.

## Entrega 2 Integrações completas por backend

Integrar as capacidades selecionadas de cada backend como um conjunto. Compartilhar alocação, handles, recursos, execução, erros e destruição em todo o módulo. Depois gerar os componentes e APIs sobre esse conjunto, em uma mesma entrega.

| Módulo | Reaproveitamento | Conjunto a integrar |
|---|---|---|
| Física | Jolt já presente | Corpos, shapes, materiais, joints, sensores, queries e personagem; ragdoll ligado à animação |
| Áudio | miniaudio já presente | Clips/streams, fontes, listeners, roteamento, efeitos selecionados, parâmetros e snapshots |
| Navegação | Recast/Detour | Construção de navmesh, persistência, consultas, agentes, obstáculos, links e debug |
| Animação | Runtime existente e Ozz onde substituir trabalho real | Clips, sampling/blending e integração com estado, eventos, rig e autoria |
| Interface de jogo | RmlUi como candidata | Documento UI, layout, controles, eventos, texto e integração de input/render |
| Efeitos | Effekseer como candidata | Recurso de efeito, reprodução, parâmetros, recursos GPU, pausa e remoção |
| Recursos gráficos | Importadores/renderer existentes; xatlas para UV quando necessário | Recursos e propriedades suportadas pelo backend, reimportação e referências persistentes |

Fontes oficiais: [Jolt](https://github.com/jrouwe/JoltPhysics), [miniaudio](https://github.com/mackron/miniaudio), [Recast](https://github.com/recastnavigation/recastnavigation), [Ozz](https://github.com/guillaumeblanc/ozz-animation), [RmlUi](https://github.com/mikke89/RmlUi), [Effekseer](https://github.com/effekseer/Effekseer), [xatlas](https://github.com/jpcy/xatlas).

O backend fornece comportamento; o gerador fornece o código repetido ao redor dele. Ainda é necessário escrever uma vez a integração semântica: quem possui recursos, quando aplica mudanças, como converte coordenadas e como encerra. Isso não se repete integralmente para cada componente.

O catálogo do módulo é definido antes da integração e contempla seus requisitos, em vez de adicionar uma propriedade isolada a cada rodada. Uma API que não corresponde a uma função efetiva provoca erro de geração/compilação ou fica explicitamente pendente; nunca ganha implementação vazia para completar a tabela.

## Entrega 3 Portes de comportamento e autoria especializada

Pesquisar uma vez por sistema e trabalhar com uma referência fixa. Usar documentação/XML para pré-preencher nomes, propriedades, enums e relações. Conferir semântica e consumidor no código-fonte. Copiar listas de propriedades não transfere implementações.

Quando houver um algoritmo portável sob licença compatível, incorporar o código com suas dependências mínimas e origem registrada. Quando a classe depender de um mundo inteiro da engine de origem, usar a arquitetura como referência e manter a implementação necessária na attachsEngine. Não construir uma segunda SceneTree apenas para aproveitar uma classe.

Godot 4.5 oferece referências concretas para registro de tipos e aplicação das propriedades: [ClassDB](https://github.com/godotengine/godot/blob/4.5/core/object/class_db.h) e [RigidBody3D](https://github.com/godotengine/godot/blob/4.5/scene/3d/physics/rigid_body_3d.cpp). UnityCsReference é código de referência com condições próprias, não um acervo para copiar livremente. [README oficial](https://github.com/Unity-Technologies/UnityCsReference).

Os itens sem solução pronta continuam incluídos: prefabs e variantes, composição de eventos/tweens, grafo de animação, timeline, ferramentas 2D, terreno, streaming e bake. O esforço vai para essas lacunas comportamentais, enquanto o gerador resolve sua exposição repetitiva.

NÃO IREI SER SIMPLISTA NO DESIGN.

Reaproveitar os controles e comandos do editor para propriedades comuns. Implementar os editores especializados por conjunto: uma ferramenta de curvas compartilhada; uma superfície de grafo reutilizável onde a interação for equivalente; timeline, mixer e pintura com suas operações próprias. Compartilhar infraestrutura não significa forçar todos a usar a mesma tela.

A autoria precisa acompanhar o runtime. Importar um efeito pronto do Effekseer não encerra edição de partículas no celular. RmlUi desenhar um documento não encerra criação visual, IME ou acessibilidade. Ozz avaliar clips não encerra o Animator. Esses trabalhos entram na entrega do sistema correspondente, sem desaparecer da estimativa.

## Entrega 4 Pacote integrado para uso

As entregas anteriores convergem para uma revisão utilizável do repositório com:

- Código-fonte, versões fixadas dos terceiros e geração reproduzível.
- Componentes criáveis e editáveis, APIs utilizáveis e recursos persistentes.
- Projetos de exemplo que combinam sistemas, em vez de uma demo isolada por classe.
- Build Android, SDK e exportação de jogo coerentes com a mesma revisão.
- Relatório curto de lacunas efetivas; nenhuma promoção por quantidade de nomes gerados.

Usar cenas compostas para verificar muita coisa por execução: personagem com animação/física/câmera/HUD; ambiente com áudio/luzes/recursos; agentes com navegação e obstáculos; cena 2D; salvar/reabrir e remoção de recursos durante Play. Acrescentar verificações específicas apenas onde esses cenários não cobrem o comportamento.

Geração e verificação de consistência podem ser automatizadas em lote. Evidência de comportamento continua vindo da execução. Build de host, APK e aparelho são resultados distintos. Nesta rodada de planejamento nenhum desses testes foi executado.

## Cobertura do conjunto pedido

Usar as 91 identidades do catálogo já versionado como escopo de entrega, não inventar outro backlog. O agrupamento operacional é:

| Destino principal | IDs |
|---|---|
| Composição, execução e recursos comuns | F001–F011, F076–F078, F082 |
| Assets, renderer e recursos gráficos | F012–F034, F079–F081, F083 |
| Física | F035–F045 |
| Input e UI | F046–F057 |
| Áudio | F058–F061 |
| Animação | F062–F066 |
| Navegação e percepção | F067–F070 |
| 2D e partículas | F071–F075 |
| Ferramentas, inspeção, código e exportação | F084–F087 |
| Módulos condicionais existentes no catálogo | F088–F091 |

Os quatro itens condicionais não são silenciosamente descartados nem tratados como prontos. Rede, XR, vídeo e extensões físicas exigem seus backends e ambientes próprios. Um gerador não cria transporte, tracking, decoder ou solver ausentes.

## Regras para a execução não voltar a ser lenta

1. A primeira mudança entrega automação aplicada a um lote real do catálogo existente. Nenhuma rodada exclusiva de frameworks ou documentação.
2. Pesquisar por módulo e fixar as referências. Não repetir a mesma pesquisa para cada componente derivado.
3. Implementar e expor conjuntos de capacidades do backend. Não fragmentar uma integração em uma conversa por propriedade.
4. Acumular mudanças coesas e compilar/validar o lote; falhas localizadas recebem verificações localizadas.
5. Preservar formatos, renderer e linguagem existentes durante esse trabalho. Substituições amplas só entram quando necessárias ao objetivo.
6. Encerrar rodadas com código integrado e artefatos quando a execução estiver autorizada. Planilhas, contagens e documentação acompanham o resultado, não o substituem.

Não há evidência para afirmar que todo o catálogo ficará pronto em poucos dias. A hipótese de aceleração é verificável: o primeiro lote deve reduzir quantos lugares precisam ser editados para acrescentar capacidade real. Se o gerador exigir tantos arquivos e decisões quanto o método antigo, simplificá-lo antes de expandir.

**Recomendação final:** produção em lote sobre a attachsEngine existente, com geração anterior aos descritores, integrações por backend e esforço manual concentrado em comportamento novo. Isto substitui a proposta anterior de começar por um pequeno piloto de áudio e continuar família por família sem mudar a forma de produzir.
