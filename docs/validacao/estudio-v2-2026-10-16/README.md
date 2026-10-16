# Estúdio v2, Criar › Personagem e Biblioteca — bateria pela UI no aparelho (16/10/2026)

Aparelho: Astra Dev `dev.aether.editor.u07` code 90 (commits 9b8a4f69 + b976af06),
projeto "Kyle – Carregar caixa" (Robot Kyle do UMotion, mesa, caixa, interação),
tudo por toque via ADB com captura antes de cada passo. Capturas nesta pasta
(1386 px de largura; originais em `build/device-rig/v90_*`).

| Passo pela UI | Resultado | Captura |
|---|---|---|
| Cena › Malhas do projeto | Biblioteca abre no trilho **Projeto** com 7 cartões (primitivas + Robot2), selo pronto e "Na cena" no canto | `v90_biblioteca.png` |
| Play da cena (carregar com as duas mãos) | Kyle pega a caixa e a mantém no ponto de carregar por 40 s; sem atravessar a mesa | `v90_play1.png`, `v90_loop_sheet.png` |
| Robot Kyle › ⋮ › Animar | Estúdio abre posando, com o personagem inteiro enquadrado | — |
| Árvore › Neck | Painel do osso com Posição/Rotação/Escala, linhas do osso na timeline, chaves-mestras na régua | `v90_st2.png` |
| Régua em 1 s › campo Rotação Z › teclado › 45 | Chave em 1 s nas quatro componentes do quaternion (auto-key), losango na régua, cabeça girada | `v90_st5.png` |
| ▶ do estúdio | Interpola (44° em 1,064 s), loop | `v90_st6.png` |
| ⋮ do estúdio | Parar de posar, Novo clipe (3), Renomear, Duração, Camadas, Curvas, Eventos, Rig, Assistente de IK, IK permanente, Root motion, Bake, Exportar GLB | `v90_st7.png` |
| ⋮ › Camadas | Barra de camadas "1/1 · Base" com peso, referência, mudo, solo | `v90_st8_camadas.png` |
| ⋮ › Curvas | Editor de curvas da componente selecionada com chaves | `v90_st9_curvas.png` |
| ⋮ › Eventos › + Evento | Evento em 0,96 s com barra de tempo/código/valor/opções | `v90_st11_evento.png` |
| ⋮ › Rig | Folha de rig no painel (Controles, espelho, poses, transferência, diagnóstico) | `v90_st12_rig.png` |
| ⋮ › Assistente de IK | Hierarquia 55 → 58: alvos de IK das mãos e do olhar criados | `v90_st13_assistente.png` |
| ⋮ › Root motion | Alterna sem erro | — |
| ⋮ › Bake da pose final | "Bake publicado · Clipes/Clipe…" | `v90_st16_bake.png` |
| ⋮ › Exportar GLB | "GLB exportado · Exportados/… 1 eventos (cues)" | `v90_st17_export.png` |
| ⋮ › Novo clipe | "Clipe 2" criado e aberto, sem formulário | — |
| ⋮ › Renomear clipe › "Aceno" | Título do estúdio passa a Aceno | `v90_st21_renomeado.png` |
| ⋮ › Duração › 3 | Régua até 3 s | `v90_st23_duracao2.png` |
| Left_Wrist › ⋮ › IK permanente | "Esta ponta já tem IK de dois ossos" (o assistente já tinha criado) | `v90_st25_ikperm.png` |
| Grade › Seleção | Folha "Autoria de pose" (gravação, reutilizar, seleção, travas, intervalo) | `v90_st29_selecao.png` |
| Grade › Copiar › Colar | Colar habilita depois de copiar | `v90_st32_colar.png` |
| Grade › IK | Alvo laranja na ponta, "arraste o alvo" | `v90_st33_ik.png` |
| Grade › Fixar (raiz selecionada) | Recusa com motivo: "Selecione a ponta de uma cadeia com dois ossos acima" | `v90_st34_fixar.png` |
| Criar › Personagem (code 88/89) | Seis receitas; Olhar para alvo cria "Olhar · Head" | `v89_personagem.png`, `v88_olhar2.png` |

## Defeitos encontrados nesta bateria

1. **Alvo do Olhar nascia ao lado do personagem.** O alvo era colocado 2 m ao
   longo do +Z do osso da cabeça e o LookAt girava a cabeça para apontar o
   +Z dela; no Kyle esse eixo aponta para o lado (+X do mundo). No Play o tronco
   e a cabeça torciam (`v90_loop_sheet.png`, quadros depois de 9 s). Corrigido
   no host: a frente do personagem vem da linha dos ombros (ou do +Z do objeto)
   e o eixo do osso que olha/mira é o que mais se alinha com essa frente
   (Olhar e Mirar). Conferido no `play-kyle-carry … lookat`: alvo em
   (0, 1.615, 2.018), na frente da cabeça.
2. **Fechar o Rig pelo × deixava o painel vazio** com o título "Rig" até tocar a
   seta de voltar (`v90_st25_ikperm.png`, painel direito). Corrigido: fechar o
   rig devolve o painel do osso.
3. Textos de estado ainda presentes: "Nenhum rig publicado para esta raiz"
   (rodapé do rig) e "Travas XYZ locais · intervalo inclui as fronteiras"
   (folha de autoria de pose). Ficam para a rodada de limpeza de textos.

Não reproduzido: a caixa atravessando a mesa no reinício. Em 40 s de Play
(cena) e 20 s no host, a caixa fica no ponto de carregar.
