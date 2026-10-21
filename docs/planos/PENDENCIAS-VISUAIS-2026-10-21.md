# Pendências visuais apontadas pelo dono (21/10/2026)

Origem: observação do dono no POCO, depois das capturas do conjunto mesclado
(G2 exposição, G3a, G4 nuvens, PK-3/PK-8, PK-6). Nenhuma das duas estava em plano
ou auditoria anterior. As duas seguem a regra de comparar com frames de vídeo da
Unity (prancha lado a lado, mesmo enquadramento e hora) antes de dar por pronto.

## V-1 — Rebarba branca nos contornos e sombras lavadas

**Sintoma (palavras do dono):** em personagens, casas e objetos aparece uma
"rebarba branca" em volta das silhuetas; e lugares que deveriam ser sombra ficam
"meio esbranquiçados", sem chegar ao escuro.

**Suspeitas a confirmar, nesta ordem, cada uma ligando e desligando o efeito
no POCO com a mesma pose de câmera:**
- reconstrução temporal / ampliação (Catmull–Rom, clamp de histórico): halo claro
  em borda de alto contraste é a assinatura típica de TAA/upscale sem clamp
  apertado ou com nitidez demais;
- neblina e espalhamento atmosférico somados depois do sombreamento, clareando o
  fundo atrás das silhuetas;
- ambiente do céu forte demais na sombra (sem oclusão difusa ou especular
  suficiente), deixando a sombra cinza-clara — liga com PK-1 (fonte de ambiente
  e SH) e com o G3a (oclusão especular);
- bloom com limiar baixo espalhando o céu claro sobre as bordas (G2/PK-5);
- MSAA/resolve e alfa de recorte nas folhagens e cabelos.

**Pronto quando:** prancha Unity × POCO na Vila mostra bordas sem halo e sombra
com contraste parecido; a causa está nomeada com evidência (captura com o efeito
ligado e desligado) e o custo de GPU foi medido.

## V-2 — Noite que não parece noite

**Sintoma (palavras do dono):** a noite "fica escura, blz", mas tudo continua com
brilho, parecendo claro; as estrelas precisam seguir o que a Unity faz. Comparando
com a referência, "nada igual, decepcionou".

**Pontos a investigar:**
- exposição automática (G2, já na base) adaptando para clarear a noite: na Unity
  a noite fica escura porque o Volume limita a adaptação (Limit Min/Max, curva
  de compensação) — conferir limites e compensação noturna;
- ambiente e reflexo do céu à noite: o céu noturno deve iluminar quase nada, com
  tom azulado e baixo; hoje o piso de `daylight` (0,015) e o ambiente autoral
  podem manter a cena clara — liga com PK-1/PK-2;
- luz da lua como direcional fraca e azulada, como nos ciclos de dia da Unity
  (HDRP Physically Based Sky com lua, ou segunda direcional), em vez de nenhuma;
- estrelas: densidade, tamanho e brilho por magnitude como no céu noturno do
  HDRP/UniStorm (só estudo), cintilação discreta, Via Láctea opcional;
- especular e bloom à noite: materiais e postes não devem "brilhar" além do que
  a fonte de luz justifica; postes acesos como fonte principal da praça.

**Pronto quando:** prancha Unity × POCO à noite (mesma hora, mesma câmera) com
céu, estrelas, praça iluminada pelos postes e sombras escuras; noite medida em
luminância média e comparada à referência.
