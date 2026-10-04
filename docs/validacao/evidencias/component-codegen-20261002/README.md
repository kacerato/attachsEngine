# Produção de componentes em lote: evidência da primeira entrega

Repositório: https://github.com/kacerato/attachsEngine. Origin conferido por Git e identidade privada confirmada pela API autenticada do GitHub. Branch local: `codex/gameplay-runtime`; HEAD e main remoto verificados em `97ecf56f17fe39e7d2fd527719b7fbce2528cd18`. A implementação está no working tree, sem publicação nesta entrega. Havia alterações locais anteriores; elas foram preservadas. O snapshot inicial representa esse checkout, não um build limpo exclusivo do commit remoto.

## O que mudou efetivamente

O gerador foi aplicado a 45 tipos existentes, com 10 tabelas de famílias, 60 tabelas de propriedades e 352 declarações numéricas/booleanas (16 são declarações de templates). Dessas entradas saem 704 lambdas tipadas de acesso e 189 campos/defaults nativos em 26 tipos. Lambdas geradas não equivalem a funções antes escritas individualmente: parte do código anterior já usava macros.

Os includes gerados ocupam os pontos reais de declaração de campos, propriedades e registro. O runtime, o Inspector e a persistência existentes continuam consumindo os mesmos tipos e descritores. O comando de sincronização também executa o exportador C# existente, conferindo suas 44 fachadas. Python é dependência de desenvolvimento/build, não do runtime.

## Provas executadas

1. **Preservação:** probe C++ compilado antes e depois examinou 45 schemas e 548 bindings numéricos/booleanos instanciados. Comparou matriz, API C#, defaults, alterações de propriedades e arquivos serializados. Os snapshots possuem o mesmo SHA-256: `776bface2b97593ee2097ccb4fa66864a0733c9d9c80cf469bd6b6e69c88e800`. Defaults válidos passaram save/read; drafts inválidos, como ScriptBehavior sem configuração, foram registrados sem fingir que eram carregáveis.
2. **Uma edição com efeito:** teste permanente cria cópia isolada e muda somente o contrato Camera: default 60→61 e mínimo 1→2. Dois includes mudam. O C++ é compilado e executado: default 61, limite 2, escrita 73, salvar/reabrir 73 e rejeição de 1. Nenhuma alteração experimental foi aplicada aos contratos de produção.
3. **Proteção do gerador:** 6/6 testes passaram, incluindo duplicação, campo desconhecido, default não consumido, include desconectado e saída desatualizada sem reparo silencioso.
4. **Comportamento existente:** 16/16 cenários passaram nos executáveis bulk50, audio_listener_family, fields50 e fields2d50. Cobrem consumidores de física 3D/2D, DSP de áudio, lifecycle, edição, bridge de script, Undo e persistência.
5. **Build e API:** os alvos host compilaram; o comando único `--sync-api` terminou com API C# atual e zero arquivos gerados alterados na segunda execução.

Os logs estão neste diretório. `generation.json` registra hashes de entrada, gerador e saídas; `manifest.json` registra escopo e hashes das evidências. Snapshots completos ficam localmente em `build/component-codegen/before.txt` e `final.txt`. O job novo de CI ainda não foi executado no GitHub: apenas seus comandos foram validados localmente.

## Reproduzir

Com Python, compilador C++20 e o build host configurado:

```powershell
python tools/generate-component-contracts.py --check
python -m unittest discover -s tests/tools -p test_component_codegen.py -v
python tools/generate-component-contracts.py --check --sync-api --build-dir build/editor-host
cmake --build build/editor-host --target aether_component_contract_probe aether_bulk50_tests aether_audio_listener_family_tests aether_fields50_tests aether_fields2d50_tests --parallel 3
build/editor-host/aether_component_contract_probe.exe build/component-codegen/current.txt
build/editor-host/aether_bulk50_tests.exe
build/editor-host/aether_audio_listener_family_tests.exe
build/editor-host/aether_fields50_tests.exe
build/editor-host/aether_fields2d50_tests.exe
```

## Limite e próximo avanço

Esta entrega automatiza parte substancial do catálogo existente; não cria 45 capacidades novas e não encerra a engine. Serialização especializada, métodos/eventos, enums, referências/recursos/coleções e setters com efeitos particulares continuam manuais. Novas bibliotecas/backends ainda não foram integrados. Não foi medido ganho de tempo de produção em escala; foi provado que uma edição central chega a código nativo executável e persistente.

Não houve build APK, captura de editor ou validação em aparelho. Os resultados são de host. Não houve mudança de desenho da interface. O acompanhamento futuro deve comparar contratos gerados e cadeias funcionais validadas, sem contar classes ou arquivos como capacidades concluídas.
