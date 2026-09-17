# Pesquisa Unity e plano Astra — 15/09/2026

Abra [ATLAS-UNITY.html](ATLAS-UNITY.html) para navegar offline. A aba **Quatro direções de layout** contém esquemas de composição; não são imagens da Astra executada. O arquivo é autossuficiente e incorpora os dados, sem servidor ou conexão obrigatória.

O [plano principal](../../planos/PLANO-UNIVERSALIDADE-COMPONENTES-LAYOUT-2026-09-15.md) audita o HEAD `f3801ba3`, atualiza R1–R4 e especifica os próximos pacotes. [OBJETOS-E-CAMINHOS.md](OBJETOS-E-CAMINHOS.md) apresenta receitas, dependências e caminhos de uso.

## Conteúdo

| Arquivo | Finalidade |
|---|---|
| `ATLAS-UNITY.html` | Busca por tipo/propriedade, filtros, fichas, herança, relações reversas e layouts |
| `CATALOGO-UNITY.md` | Consulta textual integral das propriedades/campos declarados e links |
| `catalogo-unity.json` | Metadados finais selecionados; sem corpos de implementação Unity |
| `resumo-cobertura.json` | Contagens, versões/hashes, dependências de pacote, exclusões e avisos |
| `api-nucleo.json` | Índices de membros de 117 páginas oficiais da API 6000.0 consultadas |
| `menus-gameobject.json` | 78 registros literais de menu, com limites explícitos de cobertura |
| `coletar_catalogo.py` | Extrator de tipos, membros, atributos e relações nas fontes fixadas |
| `coletar_documentacao.py` | Consulta da API oficial e coleta de pacotes adicionais fixados |
| `coletar_menus.py` | Extração das declarações literais GameObject/MenuItem |
| `publicar_catalogo.py` | Seleção, fechamento de tipos referenciados e publicação dos artefatos |
| `atlas.template.html` | Interface do atlas antes de incorporar os dados |

## Escopo e precisão

575 tipos Component, 150 elementos UI Toolkit, 65 tipos Volume, 61 recursos/contratos selecionados e 1.625 tipos de apoio. Total: 2.476 fichas e 14.030 propriedades/campos próprios extraídos. Tipos abstratos, auxiliares e obsoletos não são botões Add; campos públicos não são necessariamente propriedades editáveis no Inspector.

O catálogo referencia Unity 6000.0 e 21 pacotes/fontes adicionais nas versões indicadas. Não certifica todos os pacotes Unity possíveis nem compatibilidade conjunta das versões. Metadados de API/fonte não substituem revisão de defaults, condições de plataforma, CustomEditors e semântica individual. Há 134 fichas ligadas a arquivos que exigiram recuperação sintática/preprocessador. Consulte os avisos e a fonte antes de implementar.

Dependências obrigatórias por atributo, herança, referências tipadas e requisitos funcionais são relações distintas. As relações reversas extraídas não incluem toda consulta dinâmica feita em métodos, shaders, strings ou reflexão. Os roteiros funcionais por família e as receitas complementam o grafo, com pendências declaradas.

## Reprodução da pesquisa

Os scripts usam Python e os snapshots em `build/component-reference-research`, diretório de pesquisa que não integra a engine. O extrator requer `tree-sitter==0.25.2` e `tree-sitter-c-sharp==0.23.1` em `parser-libs`. Fontes iniciais do núcleo, pacotes históricos e índice de API vêm da pesquisa anterior do repositório; os scripts novos não reconstituem sozinhos esses insumos ausentes.

Ordem, com os insumos presentes: coletar documentação/pacotes; coletar catálogo; coletar menus; publicar catálogo. Os arquivos HTML/Markdown/JSON são gerados; editar as fontes dos geradores quando atualizar a pesquisa. Os hashes e revisões aparecem nos manifestos.

Nenhum script executa a Astra, seus testes, build Android ou ADB. Nesta entrega somente artefatos documentais foram produzidos. A interface HTML não recebeu validação interativa de navegador neste atendimento; o layout da engine tampouco foi revalidado.
