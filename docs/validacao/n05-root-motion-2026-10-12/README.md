# N05 pacote D — root motion no aparelho (12/10/2026)

Astra Dev code 50, projeto AnimatorFox.

- `aba-ik.png`: estúdio → aba IK com o cartão **Root motion** ao final.
- `root-motion-ativo.png`: um toque cria o componente no Fox com o osso **b_Hip_01** (encontrado pelo nome); cartão destacado "Ativo".
- `play.png`: Play sem falha. O clipe do Fox é no lugar; o deslocamento é validado no host.
  Componente desfeito após o teste.

Host: runtime de animação 69/69 (rotas transformação, Personagem e corpo dinâmico; laço; giro; eixos;
peso; diagnóstico; persistência; carregar corpo com física real), editor de clipes 63/63 (cartão
cria/liga/desliga/desfaz; osso pela estrutura; fixar ponta com um Desfazer).
