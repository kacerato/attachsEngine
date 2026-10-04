# Contratos de componentes gerados em lote

Esta é a fonte dos 45 registros existentes e das tabelas numéricas/booleanas migradas. O gerador produz C++ consumido pela engine, incluindo campos com defaults quando declarados aqui. Não é um catálogo paralelo e não interpreta propriedades durante frames.

## Uso

Com um build host CMake já configurado:

```powershell
python tools/generate-component-contracts.py --sync-api --build-dir build/editor-host --report build/component-codegen/generation.json
```

Esse comando gera os arquivos nativos, compila o exportador leve e sincroniza a fachada C#. O exportador depende de descritores e primitivas de GUID; não precisa compilar o editor, Jolt ou a suíte inteira. A primeira configuração do build segue o README do repositório, incluindo os headers Vulkan exigidos pelo projeto CMake.

Para conferir sem modificar fontes:

```powershell
python tools/generate-component-contracts.py --check
python tools/generate-component-contracts.py --check --sync-api --build-dir build/editor-host
python -m unittest discover -s tests/tools -p test_component_codegen.py -v
```

O segundo comando pode atualizar artefatos de compilação, mas apenas verifica a API. O teste de edição única precisa de um compilador C++20 acessível como `c++`; compila sua prova em diretório temporário, sem alterar a engine de trabalho.

## Edição

`families.json` define a ordem das famílias. Cada arquivo de família contém tipos reais, referências versionadas, requisitos e metadados de criação. `properties_*.json` contém as tabelas, bindings diretos e, quando existentes, declarações dos campos.

`defaults` permite compartilhar tipo, limites e apresentação entre os membros da tabela. Cada item contém sua identidade, rótulo e membro C++. Valores textuais são strings; expressões nativas explícitas usam `{"cpp": "expressão"}` e são conferidas pelo compilador. Metadados de visibilidade complexos preservam os predicados existentes. O código efetivo de leitura/escrita vem do tipo e membro, não de um nome de consumidor sem ligação.

Exemplo real de membro em `properties_constant_force.json`, herdando tipo, default e domínio da tabela:

```json
{
  "id": "force_x",
  "label": "Força X",
  "member": "forceX",
  "presentation": {"group": "Força mundo", "unit": "N"}
}
```

`field_groups` referencia as propriedades que também geram campos de dados. A ordem preserva o layout original das classes; adicionar campo exige incluí-lo no grupo apropriado. Alterar um default ou limite passa pelo mesmo registro da propriedade. Os defaults do estado nativo não ficam duplicados no header.

Não editar `native/scene/generated/*.inc`. A verificação recusa IDs duplicados, campos desconhecidos, defaults sem declaração, arquivos gerados sem inclusão, arquivos desatualizados e saídas órfãs. O build CMake passa pela verificação, inclusive em alvos explícitos que dependem do núcleo. O CI também verifica os contratos.

## Limites

Esta versão gera registros, campos escalares selecionados e bindings Number/Boolean/Enum. As tabelas Enum declaram `options` com o símbolo C++ da lista existente; o compilador confere a lista e o tipo do membro. O setter converte para o tipo real do campo, preservando enums fortes e inteiros. Defaults de enums ainda não são gerados. Métodos/eventos, referências, coleções, serializers especializados e migrações usam as implementações existentes. Bindings com setters de efeito colateral, tabelas calculadas e templates especializados permanecem manuais quando a geração direta mudaria sua semântica.

Não adicionar uma propriedade esperando que o gerador invente seu consumidor. Ele liga dados à infraestrutura existente; comportamento novo exige backend real. Preservar versões e leitores antigos ao alterar formatos, mesmo quando o writer já percorre a tabela de propriedades.

`generation.json` informa cobertura de geração e hashes, não conclusão de famílias ou validação Android. Uma declaração de template pode alimentar vários componentes; `property_declarations` conta declarações, sem multiplicá-las por instanciação. `generated_accessor_lambdas` conta funções emitidas, não esforço humano economizado: algumas já vinham de macros.

O relatório da primeira aplicação e suas evidências estão em `docs/validacao/evidencias/component-codegen-20261002/`. O incremento comprovado é automação aplicada ao catálogo existente; não representa 45 componentes novos.
