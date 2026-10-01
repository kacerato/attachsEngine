# Movimento de Character em reconstrução física

Reprodução anterior (`before-tests.txt`):0/2. Queda após rebuild perdeu velocidade: segunda janela0.25s deslocou apenas0.8m. Movimento e salto aceitos foram apagados: dx0/dy0.

Após correção (`character_rebuild-tests.txt`):4/4. Mesma queda desloca2.3m na segunda janela; intenção/salto sobre piso novo produzem dx0.033m/dy0.083m. Cenários: queda e reposicionamento explícito; sessão diferente com grafo/pose iguais; intenção/salto/apoio após rebuild; integração real EditorPlayScene/ScriptBridge/ABI/FixedUpdate/edição de corpo/reconciliação. FakeRuntime dirige os callbacks nativos, não simula Jolt ou GameWorld.

Regressões direcionadas passaram: chão4/4, CharacterMotor2/2, personagens autorados2/2, script/30–60Hz1/1, preservação de corpo1/1, quatro tipos de joint1/1, conexões físicas3D4/4, schema13/13, atlas5/5. Managed54/54 com quinze fixtures compiladas. O SDK público e os formatos de arquivo não mudaram neste pacote.

Projeto independente CharacterRebuild-20261001 contém piso físico e personagem com script editável. Após assentamento, o script solicita movimento/salto, altera atrito em FixedUpdate e verifica deslocamento em LateUpdate. Registra CHARACTER REBUILD PASS somente após o caminho real. TimeProbe baseline usa o relógio anterior; o cenário espera2s de simulação.

Manifesto registra APK instalado, hashes, SDK/atlas conferidos byte a byte, projeto enviado e estado do keyguard. `deviceCharacterRebuildPass=false` distingue build/instalação de execução física/CLR no aparelho. Nenhuma nova UI autoral, conceito visual ou ícone foi criado para esta correção de lifecycle.

[Contrato e referências](../../../planos/CHARACTER-REBUILD-2026-10-01.md). ABI30/Character3/cena16/prefab3;34 schemas,33 fachadas,57 receitas,230 ícones. P07 e o plano completo continuam parciais.
