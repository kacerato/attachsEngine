# Material físico (bloco F, F039)

Recurso do projeto `physics_material` (arquivo `.physmat`), vinculado ao Corpo físico (`astra.physics.body` v5, binding `material`).

Referências: Godot 4.5 [PhysicsMaterial](https://docs.godotengine.org/en/4.5/classes/class_physicsmaterial.html), atribuído ao corpo (`physics_material_override`); Unity 6000.0 [PhysicsMaterial](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PhysicsMaterial.html) para os modos de combinação e a precedência.

## Contrato

| Campo | Onde | Efeito |
|---|---|---|
| `friction`, `restitution` | Recurso e cópia no corpo | Valores do contato no Jolt (`mFriction`, `mRestitution`) |
| `friction_combine`, `restitution_combine` | Recurso e cópia no corpo | 0 padrão do motor, 1 média, 2 mínimo, 3 multiplicar, 4 máximo |
| `material` | Corpo | GUID do recurso; vazio = valores só deste corpo |

Num par de corpos vale o modo de maior precedência (máximo > multiplicar > mínimo > média), como na Unity. Com os dois em "Padrão do motor" o contato mantém a combinação do Jolt (atrito pela média geométrica, restituição pelo maior valor), o que preserva cenas antigas. A regra é `AetherCombinePhysicsMaterial` (`native/physics/jolt_bridge.h`), aplicada por par no `ContactListener` do bridge (`OnContactAdded/Persisted` → `ContactSettings::mCombinedFriction/Restitution`).

## Modelo de dados

Mesmo modelo do Perfil de ambiente: o corpo guarda a cópia dos valores e o GUID; o Play usa a cópia, sem ler arquivos. Recurso ausente conserva a cópia e o GUID (o Inspector diz "Material ausente · … · cópia local"). Corpo v4 lê sem material e com "Padrão do motor".

## Editor

- Aba Material do Corpo físico: material, atrito, combinar atrito, restituição, combinar restituição (nesta ordem).
- Seletor do material: "Criar material com os valores deste corpo" (grava `Física/<nome>.physmat` e o registro juntos, liga o corpo) ou, com material escolhido, "Atualizar material com os valores deste corpo", que sincroniza todos os corpos que o usam. Escolher um material copia os valores para o corpo na mesma transação.
- Atualizar o recurso entra no histórico: Desfazer volta o arquivo, o registro e as cópias.
- Ícone novo `physics/material` no atlas.

## Diferenças

| Aspecto | Astra | Classificação |
|---|---|---|
| Onde o material mora | No corpo | Equivalente ao Godot; adaptação em relação à Unity (no colisor), porque hoje cada colisor exige corpo próprio |
| Atrito estático e dinâmico | Um atrito só | Adaptação explícita: o Jolt tem um coeficiente |
| Material por colisor de um corpo composto | Não | Pendente com a composição de colisores (F038) |
| Dados de superfície (tag para som de passos, etc.) | Não | Pendente |
| Edição direta do `.physmat` em Propriedades | Não: edita-se pelo corpo e "Atualizar" | Pendente |

## Validação executada (06/10/2026)

- Host C++: `physics_material_*` 4/4 e `physics_body_v4_reads_*` 1/1 — recurso relido com nome entre aspas e recusas de valores/versão; precedência da combinação; corpo v4→v5; efeito real no Jolt: esfera com restituição 0,9 sobre chão 0 quica acima de 1,2 m no padrão e fica abaixo de 0,6 m com "Mínimo"; sessão: criar, escolher em outro corpo, atualizar sincronizando, desfazer (arquivo incluído) e recusa de GUID de fora do projeto. Suíte 1419/1423 (4 falhas anteriores à branch). C# 521/521.
- UI executável: `docs/validacao/evidencias/physics-material-20261006/` (aba Material e seletor em 853×394 e 1200×700).
- Aparelho: projeto `MaterialFisico-20261006` gerado por `aether_ui_preview write-physics-material-project`, com os dois materiais criados pelo fluxo do editor. `MATERIAL borracha=1.67 massa=0.57 PASS` (combinação Máximo quica, Mínimo para no chão), 0 erros Vulkan (`aparelho-logcat.txt`). Inspector do aparelho mostra `Física/Bola de borracha.physmat` resolvido (`aparelho-03-material.png`). As bolas não têm malha; o movimento é medido pela sonda.
