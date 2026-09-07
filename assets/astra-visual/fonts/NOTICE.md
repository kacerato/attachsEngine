# Fonte da interface

## Inter Variable

`Inter-Variable.woff2` é o **Inter**, de Rasmus Andersson, distribuído sob a
**SIL Open Font License 1.1** — texto completo em `OFL-1.1.txt`, cópia literal da
que acompanha a distribuição do Blender 4.5 (`license/spdx/OFL-1.1.txt`), de onde
este arquivo foi tirado (`4.5/datafiles/fonts/Inter.woff2`).

A OFL permite embutir e redistribuir, inclusive dentro de um aplicativo, e exige
que a licença acompanhe a fonte e qualquer trabalho derivado dela. O atlas assado
abaixo é um trabalho derivado: ele não pode ser distribuído sem esta pasta.

O nome reservado da OFL não é usado — nada aqui se chama "Inter" como se fosse
outra versão da fonte. `astra-inter-sdf` é o nome do atlas, não da família.

## Por que Inter e não Roboto

Os masters do sistema visual foram desenhados com uma geométrica de altura de x
grande e números tabulares estreitos. Roboto é tecnicamente equivalente e já
existe em todo aparelho Android, mas deixaria a interface parecida com um app
Android padrão — que é exatamente o que a identidade do ASTRA evita. SF Pro foi
descartada por licença: a da Apple restringe redistribuição.

## Atlas

`astra-inter-sdf.png` + `astra-inter-sdf.json` são gerados por
`tools/bake-font-atlas.py`. Regenerar:

```bash
python tools/bake-font-atlas.py
```

Contém ASCII imprimível (32–126) em três pesos — Regular 400, Medium 500 e
SemiBold 600 —, que é o conjunto que os mockups usam. **Não cobre acentuação.**
Um nome de objeto com "ã" mede pelo avanço de reserva e desenha o que houver;
cobrir Latin-1 ou CJK exige atlas por faixa e shaping de verdade, não uma tabela
maior.

O eixo de tamanho óptico do Inter foi fixado em 14, o desenho para texto pequeno.
Um campo de distância serve qualquer corpo, então era preciso escolher um; todo
texto real da interface é pequeno — o título grande dos mockups é a marca ASTRA,
que é imagem.
