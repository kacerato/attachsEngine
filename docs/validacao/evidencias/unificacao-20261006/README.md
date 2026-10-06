# Unificação em main — validação no aparelho (06/10/2026)

APK debug do merge `dcf5c0f5` (main), 239085867 bytes, SHA-256
`a6237c53703fc6113fe5d0fce6099a655ef2626c9a84ba4289d523cb02c27aee`, instalado com
`install -r` (projetos preservados) no Xiaomi 25053PC47G por ADB sem fio.

| Captura | O que mostra |
|---|---|
| 01-shell | Shell abre com a lista de projetos |
| 02-editor | Projeto ADBGameplayValidation aberto pela intent `astra.open_project` |
| 03/04 | Criar › Gameplay com as receitas novas e ícones próprios |
| 05 | Gatilho sonoro: Corpo físico, Colisor 3D, Audio Source, Conexão de evento |
| 06 | Criado e salvo; Inspeção abre na Conexão de evento, aba Quando primeiro |
| 07 | Aba Então inteira na tela: Chamar método, Este objeto, Áudio: tocar |
| 08 | Play em execução (ABI v45), mesmo processo, sem erro do runtime C# |
| 09 | Stop devolve a cena autoral intacta |

Achados:
- Descrição da receita Conexão de evento cortava no painel; encurtada no commit seguinte.
- A camada de validação Vulkan registra `VUID-vkCmdDraw-renderPass-02684` no
  `InstancedRenderer` (dependências de subpass incompatíveis entre o render pass
  do comando e o do pipeline). Não vem das frentes de API; fica para a frente de
  renderização investigar.

Não verificado no aparelho: som audível do Gatilho sonoro ao vivo (a cena de
validação não tem corpo dinâmico entrando no sensor nem clipe WAV atribuído),
vibração física, `ScreenPointToRay` por toque e troca de cena por script.
