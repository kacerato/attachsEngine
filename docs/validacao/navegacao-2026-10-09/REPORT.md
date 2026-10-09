# Navegação (bloco J) — aceite no aparelho

Data: 09/10/2026. Aparelho POCO F7 (onyx_global), Android 16. Canal Astra Dev
`dev.aether.editor.u07`, versionCode **27**, `0.3.0-dev.navigation.20261009.4`
(SHA-256 do APK `66b23580a82070bcab4c81089b6e9c6e87a89646fe75b9fbd2ad5fe6ea6d7786`).
Base: branch `claude/api-componentes` depois do merge do Animation Studio.
O APK público só é trocado na distribuição 0.3.0, registrada à parte.

## Cenário

Projeto `Navegacao-20261009`, gerado por `aether_ui_preview write-navigation-project`:
chão e parede estáticos, Caixote com Obstáculo, Superfície **sem malha** (o bake é
feito pela interface), Perseguidor (Personagem + Agente com alvo), Andarilho
(Agente que move a própria pose) e a sonda C# `tests/fixtures/navigation/NavigationProbe.cs`.
Uma Luz direcional foi criada pela folha Criar no próprio aparelho.

## Autoria pela interface

| Passo | Resultado | Evidência |
|---|---|---|
| Selecionar Superfície e abrir o componente | Cartão "Sem malha assada", Assar e Malha visível | `03-inspector.jpg` |
| Assar | 12 polígonos · 327 m² · 4 tiles; `Navegação/Superfície.navmesh`; Desvincular aparece | `04-baked.jpg` |
| Malha no viewport | Polígonos preenchidos por área e contorno; o vão da parede fica sem malha | `24-fill.jpg` |
| Criar → Navegação | Quatro receitas com os ícones novos | `11-nav-category.jpg` |
| Encerrar o app e reabrir o projeto | Recurso recarregado do disco e malha desenhada | `39-cold.jpg` |

## Execução (Play)

`runtime-device.log`, sete verificações, todas PASS:

1. malha pronta, 12 polígonos, destino aceito;
2. Andarilho chegou (evento `destination_reached`, 0,20 m), caminho encerrado;
3. Perseguidor alcançou o alvo contornando a parede (1,00 m = distância de parada);
4. com o alvo do outro lado, o caminho foi refeito sozinho (1,01 m);
5. Warp para (−7, 0, −7) a 0,00 m;
6. destino fora da malha recusado, estado 3;
7. evento `path_failed` entregue no quadro seguinte.

`play-final.mp4` (21,9 s, 2.018 quadros) foi revisado em folha de 1 quadro/s: o
Perseguidor contorna a parede pela ponta norte, volta quando o alvo troca de lado e
o Andarilho cruza o fundo da cena.

## Defeitos encontrados no aparelho e corrigidos

- **Métodos da navegação pelos scripts** respondiam "não há mundo de execução":
  a ponte de scripts monta o próprio conjunto de serviços e não tinha a navegação.
  Corrigido e coberto por `component_operations_navigation_methods_reach_scripts_through_abi`.
- **`CreatePrimitive` sob Personagem, corpo móvel ou Agente** criava um corpo
  estático que a física recusa (o Play parava) ou que travava a pose do agente.
  A primitiva nasce só visual nesses casos, como o filho visual do editor
  (`script_primitive_under_character_moving_body_or_agent_is_visual_only`).
- **Preenchimento da malha** não aparecia: triângulos da interface amostravam o
  texel (0,0) do atlas imediato. A lista de desenho agora conhece o texel branco da
  ImGui (`addSolidTriangle`); isso também corrige o preenchimento dos nós de grupo
  do Animator.
- Ícones de navegação na Hierarquia e nomes curtos das receitas (os longos ficavam
  cortados nos cartões).
- A recusa de pose de um agente com corpo em filho vira diagnóstico explícito.

## Host

Nativa **1527/1527**; gerenciada 523 passaram, 0 falharam (71 pulados, incluindo o
SDK de autoria de animação sem a ponte nativa no host); Android arm64 compilado
pelo Gradle.

## Não validado aqui

Links e obstáculos em movimento foram validados no host (`navigation_*`), não no
aparelho; o Caixote recortou a malha sem medição dedicada. Sem medição de
desempenho sustentado, multidões grandes ou navmesh de cenas importadas grandes.
