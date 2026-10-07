# U07: movimento físico, animação e câmera — correção do laboratório

## Contrato do bloco

O usuário encontrou repouso congelado, deslizamento, caminhada no ar, controle
aéreo fraco e câmera atravessando o piso. Este bloco corrige esses comportamentos
e entrega apenas o laboratório como projeto embutido. Não encerra todo U08.

## Cadeia real

- Jolt / CharacterMotor → `MotorMotionState` pela família opcional
  `astra.motor-motion` v1 → `MotorAnimationDriver` → recursos importados →
  misturador existente → hierarquia de ossos → paletas deformáveis.
- O estado expõe passo medido, apoio, velocidade, velocidade do contato,
  normal, ponto e ID do suporte. Identidades aposentadas são recusadas.
- Repouso é um clipe real, sempre avançando. Walk/Run compartilham fase de
  passada e pesos; a fase usa velocidade relativa ao suporte e distância do
  ciclo. Histerese evita alternar repouso/movimento por ruído.
- Impulso, subida, ápice, queda e aterrissagem usam apoio físico e velocidade
  vertical. Não inferem chão por altura do objeto nem mantêm caminhada no ar.
- Cadência/fase/repetição de AnimationState preservam fades em andamento.
- Body mantém posição, velocidade e rotação do ator. O script envia somente
  intenção e velocidade angular Y. `airControl=.85` atua no solver; a malha e
  as formas medidas no objeto permanecem vinculadas ao mesmo Body.
- Canvas → câmera real/Cérebro → câmera virtual orbital. Gesto escopado se
  soma à entrada global; CameraLook não escreve uma segunda pose no Cérebro.
- Varredura de esfera existente consulta a física, ignora o Body do alvo,
  aproxima imediatamente diante de obstáculo e retorna com amortecimento.
  O laboratório usa raio .25 m, distância mínima .1 m e plano próximo .05 m.
- O destino de hardware segue o alvo da câmera virtual efetivamente ao vivo.
- O APK exclui os dez pacotes antigos e embute o projeto independente completo.
  A migração remove somente nomes históricos com descriptor, thumbnail de
  exemplo, README e registro de assets históricos; projetos do usuário ficam.
- Botão de Play e autostart aguardam publicação Current dos scripts. Catálogo
  Empty não é confundido com assembly pronto na primeira abertura.

## Recursos e importação

Godot TPS Demo, revisão `a82f15448e9b015440d3bbdf5e10801b260c4e9f`:
https://github.com/godotengine/tps-demo/tree/a82f15448e9b015440d3bbdf5e10801b260c4e9f

Humanoide de teste com 145 juntas e oito clipes reais. A derivação elimina o
track dedicado de root motion, porque o movimento é produzido por forças.
Conserva as malhas, demais canais e bind pose; compacta apenas buffers não usados.
As passadas originais medem 1.75 m e 2.666667 m após escala autoral. Fonte,
revisão, SHA, alterações e atribuição CC-BY-3.0 acompanham o asset.

Três ossos quase alinhados a -90° expuseram instabilidade na decomposição.
Uma base ortogonal calculada em double recupera ângulos consistentes, mas a
matriz original continua obrigada a passar pela mesma checagem de reconstrução:
cisalhamento e reflexão incompatíveis continuam recusados.

## Referências e princípios

Godot 4.5 [CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html):
velocidade real, normal do piso e velocidade da plataforma. A adaptação usa
dados medidos pelos motores já existentes; não substitui Jolt por controlador fake.

Godot 4.5 [AnimationTree](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_tree.html):
mistura e transições condicionais. A adaptação é um controlador tipado reutilizável
sobre o misturador existente; não anuncia editor visual de AnimationTree.

Godot 4.5 [SpringArm](https://docs.godotengine.org/en/4.5/tutorials/3d/spring_arm.html)
e Unity Cinemachine 3.1 [Deoccluder](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineDeoccluder.html):
pivô independente do facing e consulta volumétrica contra geometria física.

## Aceite exigido

Importar e salvar os oito clipes → abrir no aparelho → repouso → acelerar e
soltar joystick → voltar a repouso → saltar parado e andando → mudar direção
no ar → aterrissar → orbitar apontando a posição desejada abaixo do piso →
manter câmera fora da geometria → salvar/reabrir. Inspecionar todos os frames
codificados da captura final e confirmar o único pacote predefinido no APK.
Resultados e hashes finais ficam no relatório de validação, sem converter este
plano em prova de execução.

## Limites explícitos

Não é IK de pés, root motion em runtime, retargeting entre rigs, navegação,
blend tree visual universal ou animação procedural. A cadência acompanha a
distância física, mas não promete pés perfeitamente presos em terreno irregular.
Colisão de câmera requer física, filtros e raios apropriados; não é uma garantia
contra toda malha sem collider, sensor, spawn penetrado ou configuração inválida.
No pitch extremo contra o piso, a retração pode recortar o personagem;
enquadramento automático/fade de oclusores não está implementado.

## Fechamento deste contrato

12/12 direcionados, 121/121 regressões no reteste, ProjectCompiler 1/1 e ProjectStore
7/7. Validação e principal instalados; 527/527 quadros examinados em 67 páginas.
Save/reabertura fria e serializer nativo confirmados; dez defaults retirados e
hashes dos projetos do usuário preservados. Falhas/transientes e limites no
[relatório](../../validacao/u07-2026-10-07/REPORT.md).
