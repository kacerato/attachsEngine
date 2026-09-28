using System;
using Astra;

[ComponentId("acceptance.tags.driver")]
public sealed class TagProbe : Behavior
{
    private static void Require(bool value, string message)
    {
        if (!value) throw new InvalidOperationException("TAGS FAIL: " + message);
    }

    public override void Start()
    {
        var parent = Object.Parent!.Find("Parent")!;
        var child = parent.Find("Child")!;
        Require(child.Tag == "Alvo", "atribuição do Inspector persistida");
        parent.Tag = "Untagged";
        Require(child.CompareTag("Alvo") && !child.ActiveInHierarchy, "comparação no objeto inativo");
        Require(Object.FindWithTag("Alvo") is null, "busca exclui hierarquia inativa");
        parent.SetActive(true);
        Require(Object.FindWithTag("Alvo")?.ObjectId == child.ObjectId, "busca global fora da subárvore do Driver");
        Require(Object.FindGameObjectsWithTag("Alvo").Length == 1, "snapshot ativo");
        child.Tag = "Missão";
        Require(child.Tag == "Missão" && child.CompareTag("Missão"), "UTF-8 atravessa C#/ABI/C++");
        Require(Object.FindGameObjectsWithTag("Alvo").Length == 0, "troca atualiza consulta");
        var errors = 0;
        try { child.Tag = "Desconhecida"; } catch (WorldException) { ++errors; }
        try { child.CompareTag("Desconhecida"); } catch (WorldException) { ++errors; }
        try { Object.FindWithTag("Desconhecida"); } catch (WorldException) { ++errors; }
        Require(errors == 3 && child.Tag == "Missão", "tag indefinida recusa sem alterar");
        var temporary = Object.CreateChild("Temporário");
        temporary.Tag = "Missão";
        Require(Object.FindGameObjectsWithTag("Missão").Length == 2, "objetos criados participam");
        temporary.Destroy();
        Require(Object.FindGameObjectsWithTag("Missão").Length == 1, "destruição pendente excluída");
        try { _ = temporary.Tag; } catch (WorldException) { ++errors; }
        Require(errors == 4, "referência destruída recusada");
        Scene.Log(ObjectId, "TAGS PASS: Inspector persistido; UTF-8; busca global ativa; troca; erros; destruicao");
        Enabled = false;
    }
}
