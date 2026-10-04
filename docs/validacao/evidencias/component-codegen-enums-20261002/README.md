# Segundo incremento: enums conectados ao catálogo

Repositório conferido: https://github.com/kacerato/attachsEngine.git. Alterações locais sobre a entrega anterior; sem push.

## Avanço medido

| Medida | Antes | Depois |
|---|---:|---:|
| Declarações de propriedades geradas | 352 | 374 |
| Tabelas de propriedades geradas | 60 | 74 |
| Propriedades Enum geradas | 0 | 22 |
| Lambdas de acesso geradas | 704 | 748 |
| Tipos registrados | 45 | 45 |
| Campos/defaults gerados | 189 | 189 |

Os bindings gerados usam os tipos reais, opções existentes e metadados de apresentação. Abrangem Camera, AudioSource, Collider, PhysicsBody, Body2D, Collider2D, Joint2D, conexões de eventos 2D/3D, PathFollow, Timer, restrições e tween. Não foram criadas novas capacidades. A entrega reduz mais um grupo de callbacks repetidos e mantém o registro comum do Inspector/API.

Setters especializados de animação, ambiente, joints, luzes, LOD e skinned mesh permanecem manuais. Por exemplo, mudar o tipo de Joint ajusta motor e limites; esse comportamento não foi substituído por uma atribuição simples. Listas de opções e defaults de enums permanecem no C++.

## Evidência

O probe foi ampliado antes da migração e compilado nos dois estados. Examina 45 schemas, 548 bindings numéricos/booleanos e 530 opções enumeradas, registrando escrita/leitura, validade e conteúdo serializado. Snapshot antes/depois idêntico: SHA-256 `3840753e7d45a49125e10a509b49dd983f4b01b8d39213b7178d9016eb4a75fd`. Isso é comparação de comportamento, não prova de todas as combinações de estado. Snapshots locais: `build/component-codegen/enum-before.txt` e `enum-after.txt`.

Os seis testes do gerador passaram. O teste C++ da Camera agora também percorre o binding Enum gerado: escreve Orthographic, salva/reabre, confere o valor e rejeita modo 42 pela validação real. A API C# foi conferida com o exportador compilado e permanece idêntica.

O build host concluiu e os 8/8 cenários de bulk50 e audio_listener_family passaram: física, criação, Undo, persistência, bridge de script e DSP real de áudio. Os logs acompanham este arquivo. Não houve execução no Android ou CI remota. Não houve alteração visual ou nova biblioteca/backend. Métodos/eventos continuam como próximo problema de integração na ABI/runtime; não foram contabilizados como concluídos.

Reprodução: os mesmos comandos da primeira entrega, com o probe atualizado e os alvos `aether_bulk50_tests` e `aether_audio_listener_family_tests`. `generation.json` contém a cobertura e hashes atuais. Os resultados da primeira entrega permanecem históricos em seu diretório, sem sobrescrita.
