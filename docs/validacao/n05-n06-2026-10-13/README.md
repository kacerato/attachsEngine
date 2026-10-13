# N05 + N06 — validação (13/10/2026)

Host (Windows/MinGW, -g1 + suítes em paralelo): aether_tests 1557/1557, runtime de animação 77/77,
editor de clipes 64/64, GUI 126/126, bulk 4/4.

Aparelho (Astra Dev code 51–54, projeto AnimatorFox):
- `aba-ik-bake-exportar.png`: aba IK com Root motion, Bake da pose final, Exportar GLB e política de root motion.
- `exportar-glb.png`: exportação GLB no aparelho — 30 nós, 15 canais, taxa elevada automaticamente para 240 Hz,
  reimportação comparada em 7200 amostras: 0,36 mm / 0,056°.
- Bake no aparelho (code 51–53) revelou dois problemas reais: canais com correção constante não eram gravados e
  o relatório media em unidades do modelo. Corrigidos; o bake final foi verificado no PC com o projeto real copiado
  do aparelho: 481 quadros a 240 Hz, 0,06% do tamanho, maior desvio na cabeça aos 1,84 s (movimento rápido da pose
  resolvida). A tela do aparelho bloqueou antes da repetição no code 54.

Pendente por ADB: repetir o bake no aparelho com o code 54; locomoção (voo, nado, dirigir, agachar) em cena real.
