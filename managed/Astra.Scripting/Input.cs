using System.Numerics;

namespace Astra;

/// <summary>Os papéis que o projeto atribui às próprias ações.</summary>
public enum InputRole : uint
{
    Move = 0,
    Look = 1,
    Jump = 2,
}

/// <summary>
/// As ações de entrada vistas pelo projeto.
///
/// O núcleo não conhece "Mover" nem "Saltar": quem nomeia as ações, escolhe o
/// que as aciona e diz quais papéis elas cumprem é o projeto, no recurso de
/// ações. Um comportamento que queira "a ação de andar deste jogo, seja qual
/// for o nome" pergunta por <see cref="RoleAction"/>.
///
/// Quando a interface do editor está com o toque (IDE aberta, teclado, modal,
/// gizmo arrastado) o gameplay perde o foco: todos os eixos leem zero e nenhum
/// botão fica preso.
/// </summary>
public readonly struct InputAccess(ISceneAccess scene)
{
    /// <summary>Eixo 2D da ação. Ação desconhecida lê zero.</summary>
    public Vector2 Axis2(string action) => scene.InputAxis(action, out var value) ? value : Vector2.Zero;

    /// <summary>Componente X do eixo, para ações Axis1D.</summary>
    public float Axis(string action) => Axis2(action).X;

    public bool Pressed(string action) => scene.InputButton(action, 0) == 1;
    public bool JustPressed(string action) => scene.InputButton(action, 1) == 1;
    public bool JustReleased(string action) => scene.InputButton(action, 2) == 1;
    /// <summary>Falso quando a ação nem existe no mapa do projeto.</summary>
    public bool Exists(string action) => scene.InputButton(action, 0) >= 0;

    /// <summary>Nome da ação que o projeto atribuiu a este papel, ou vazio.</summary>
    public string RoleAction(InputRole role) => scene.InputRole((uint)role);

    public Vector2 Move => Axis2(RoleAction(InputRole.Move));
    public Vector2 Look => Axis2(RoleAction(InputRole.Look));
    public bool JumpPressed => JustPressed(RoleAction(InputRole.Jump));

    /// <summary>
    /// Liga ou desliga um contexto. Uma ação em contexto desligado lê zero sem
    /// perder o que o usuário configurou — é assim que um menu do jogo silencia
    /// o controle do personagem sem apagar o mapa.
    /// </summary>
    public void SetContextEnabled(string context, bool enabled) => scene.InputContext(context, enabled ? 1 : 0);
    public bool ContextEnabled(string context) => scene.InputContext(context, -1);
}
