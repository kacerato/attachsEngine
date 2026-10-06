# Fontes físicas da hierarquia

Projeto inicial produzido pelo serializer/importador nativo da attachsEngine. Copie a pasta inteira, inclusive `.astra`, para `Projetos` do editor. Abra **UI Hierarchy Sources v1**.

`CompoundPlayer` não tem renderer nem física. Seus descendentes usam duas instâncias da mesma Mesh: uma rotacionada sob um ramo com escala não uniforme, outra rotacionada/escalada diretamente. A câmera e o Canvas de controles apontam para `CompoundPlayer`.

Selecione `CompoundPlayer` → menu do Inspector → **Configurar locomoção** → **Decompor** → **Fontes**. Escolha **Disponíveis**, confira as duas fontes e conclua. Gere com **Equilibrado · 16**, revise as partes e aplique. Um Undo restaura o objeto inicial; visuais e filhos permanecem independentes. Salve e entre em Play: joystick, salto e impulso controlam o Body principal.

Os controles são autorados em `UI/main.aeui`, com comportamento em `Scripts/GuiAuthoredControls.cs`; não são controles globais obrigatórios. O projeto pronto está em `../HierarchySourcesReady`.

Limites e evidência: `docs/planos/ui-universal/U05-FONTES-HIERARQUIA-2026-10-05.md` e `docs/validacao/ui-universal-2026-10-05/hierarchy-sources.json`. Alterar fontes/poses após gerar exige nova autoria; skin deformada, regeneração de compostos com overrides, espelhamento autoral e edição por cortes permanecem abertos.
