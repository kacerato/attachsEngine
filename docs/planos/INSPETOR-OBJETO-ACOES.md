# Inspetor — configurações universais e ações do objeto

14/09/2026 · branch `codex/gameplay-runtime`, sobre `630c2f19`. Pedido do usuário: levar mais ações para o painel de propriedades, no padrão Unity/Godot (o objeto selecionado se edita onde estão as propriedades dele). O layout aprovado do IDE não foi redesenhado: tudo entra em cards e menus que o inspetor já tinha.

## O que mudou

| Onde | Antes | Agora |
|---|---|---|
| Card **Objeto** (logo depois de Transformação) | não existia; visibilidade só no olho da hierarquia e camada sem UI | Nome (toque renomeia), Visível, Projetar sombra (só em objeto com malha), Camada (percorre as camadas **nomeadas** do projeto) e uma linha de informação (ID e quantidade de filhos) |
| **⋮ do cabeçalho** do inspetor | abria só renomear | lista de ações: Renomear, Duplicar, Excluir, Enquadrar na vista, Criar filho vazio, Mover acima/abaixo, Mudar pai, Mover para a raiz, Copiar/Colar/Redefinir transformação. Ações que não valem para a raiz da cena aparecem apagadas |
| **⋮ do card Transformação** | não existia | Copiar, Colar, Redefinir posição, rotação, escala ou tudo |

Todas as ações passam pelo mesmo histórico (um passo de desfazer cada). O menu de entidade da hierarquia continua existindo.

## O que ficou de fora de propósito

- **Estático** e **Receber sombra** existem no documento, mas nenhum consumidor os lê (renderer, física e runtime ignoram). O plano proíbe controle sem consumidor, e um teste existente já exige que não apareçam. Entram quando um sistema passar a usá-los.
- **Tag/grupo**, **modo de processamento** e similares de Unity/Godot não têm consumidor na Astra.
- Reordenar componentes (Mover para cima/baixo, como na Unity) não foi feito: a ordem dos componentes não afeta nada hoje.

## Evidência

Host: teste `inspector_object_card_and_actions_edit_the_selected_object_through_history` — por toque, card Objeto alterna Visível com desfazer, escolhe a próxima camada nomeada, card Transformação copia/redefine posição/cola, e o ⋮ do cabeçalho duplica e cria filho vazio sob o selecionado. Suíte **878/880** (as duas falhas antigas de console/barra).

Aparelho (APK `821FA1F2F9CE1A62776DC4458C60D316A4A24ABAB95255EAC2FEDBA0989072F8`, instalado sem apagar dados, projeto `M08Recursos0913k`, capturas só com o editor em primeiro plano; nenhuma ação que altere o projeto foi executada nesta conferência):

1. Objeto selecionado por toque na hierarquia: inspetor com **Transformação** (agora com ⋮), **Objeto** e **Malha**. [Captura](../validacao/evidencias/inspetor-20260914/cards-transformacao-objeto-malha-adb.png).
2. Card Objeto aberto: Nome, Visível, Projetar sombra, Camada "Padrão" com setas e "ID 3 · 0 filhos". [Captura](../validacao/evidencias/inspetor-20260914/card-objeto-adb.png).
3. ⋮ do cabeçalho: as 12 ações, com "Colar transformação" apagado enquanto não há cópia. [Captura](../validacao/evidencias/inspetor-20260914/acoes-do-objeto-adb.png).
4. Fechar pelo ⋮ devolve os cards. [Captura](../validacao/evidencias/inspetor-20260914/menu-fechado-adb.png).

O menu do card Transformação e a execução das ações foram verificados no host pelo teste acima, não por toque no aparelho.

Durante a implementação, um script de substituição removeu por engano a linha que criava o card Transformação (precedência de operadores do PowerShell); seis testes de sessão acusaram e a linha foi restaurada antes de qualquer commit.
