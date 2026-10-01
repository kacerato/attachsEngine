# Personagem: degraus, apoio no chão e gravidade — Character3

Referências oficiais: [Unity6000.0 CharacterController](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html), [Godot4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) e [Jolt CharacterVirtual no commit vendorizado](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Character/CharacterVirtual.h). A Unity distingue limite de inclinação e Step Offset da forma da cápsula; Godot expõe distância de snap e documenta seu comportamento em movimento ascendente. Jolt ExtendedUpdate combina Update/WalkStairs/StickToFloor com vetores explícitos. A Astra conserva sua cápsula virtual e sua autoridade de pose, sem transformá-la em Rigidbody.

## Dependências reais

Criação/Inspector → Character3 → arquivo/prefab existentes → ScenePhysics/CharacterMotor → AetherPhysics_UpdateCharacterEx → ExtendedUpdateSettings real do Jolt → contatos/velocidade/base da cápsula → pose mundial/local → viewport/câmera. Gravidade é integrada pelo motor, como exige a API Jolt; o parâmetro gravity de ExtendedUpdate não integra sozinho a velocidade vertical.

`step_height` e `floor_snap_length` são distâncias de mundo em Y; zero desliga o algoritmo correspondente. Defaults0.4/0.5 conservam o Jolt anteriormente fixo. `gravity` é aceleração descendente do personagem, default9.81 conserva o ScenePhysics anterior; o motor privado mantém seu default24. Zero interrompe aceleração futura e conserva a velocidade já adquirida. Não promete pairar após cair. Subida por salto não ativa snap: a política vem do ExtendedUpdate real.

Versões1–2 migram defaults, sem reordenar os seis números antigos; v3 acrescenta três campos. Propriedades de chão/gravidade/velocidade/salto alteram o motor existente a cada subpasso seguro e não pedem reconstrução de cápsula/mundo. Forma e inclinação continuam na política de reconstrução existente. Jump lê o valor atual do componente no próprio comando; não depende de um cache atualizado depois da chamada de script.

Distâncias0–10m e gravidade0–1000m/s² são limites Astra. Não foi transplantada a restrição de Step Offset≤altura da Unity: o backend Jolt usa distância de busca e cenas antigas já tinham0.4m mesmo em cápsulas pequenas. Essa distinção conserva a compatibilidade e é verificada na migração.

Snapshots de execução não entram no documento autoral. A API ABI30 existente de propriedade tipada e as fachadas geradas expõem os novos campos; não foi criada outra VM ou controlador. Há limites explícitos para distâncias/gravidade e recusa antes de alterar settings. Nenhuma propriedade foi qualificada como numeric tween de física.

## Editor

NÃO IREI SER SIMPLISTA NO DESIGN.

Categoria Chão mantém as duas distâncias próximas; gravidade fica em Locomoção. A proposta estrutural é ligar esses controles à medida espacial no viewport: cápsula vertical real e segmentos acima/abaixo do pé, só detalhados na seleção. Os dados vêm do componente. O marcador usa o novo ícone physics/character-ground, SVG integrado ao atlas230; não cria painel permanente nem altera a identidade do tipo no catálogo.

O gizmo segue a cápsula world-Y do runtime, não a rotação arbitrária do objeto. Não é um debug de contatos nem resultado de queries; ele mostra a forma configurada e as distâncias de busca. Capturas host e teste de interação devem distinguir autoria configurada de resultado físico.

## Aceite e limites

Piso e degrau30cm, dois personagens reais: degrau desligado bloqueia; ligado ultrapassa a borda. Borda descendente com gravidade zero separa busca de apoio de aceleração gravitacional. Edição ao vivo de gravidade conserva velocidade sem reconstrução; Inspector/IME/histórico/arquivo integram autoria. A fixture C# compara as duas travessias sobre Jolt, sem alterar engine ou depender de exemplo hardcoded no produto.

[Evidências](../validacao/evidencias/character-ground-v3-20261001/README.md). Character3/ABI30/cena16/prefab3;34 schemas,33 fachadas,57 receitas,230 ícones. Três propriedades autoráveis novas no tipo existente. Não conclui personagem inteiro, P07 ou o plano: crouch autoral, teleport com refresh de contatos, parâmetros de contato/push, outras políticas de suporte e qualificação física Android continuam separados.
