# Unity — galeria oficial de componentes, objetos e Inspector

Consulta: 26/09/2026. Referência: Unity 6.0 (6000.0), com as versões de pacotes fixadas no inventário do projeto.

[Abrir galeria](GALERIA.html). Use a busca, os filtros de área/pacote e o botão **Todas as imagens**. Desmarque **Com imagens** para ver todas as fichas. Clique na imagem para abrir sua resolução original.

São **575 fichas de componentes**, **196 com imagens localizadas**, e **459 imagens oficiais distintas** contando também objetos e Inspector. Verificação HEAD: 216 endereços responderam HTTP 200.

## Limites da entrega

- Imagens oficiais remotas; precisam de internet. Não foram baixadas.
- 575 fichas reproduzem a classificação do inventário; incluem bases, abstratos, obsoletos e tipos ocultos.
- A imagem pode ser exemplo, diagrama ou captura do Inspector. Não é captura local do Unity.
- Ausência de imagem significa não localizada nas páginas consultadas; não prova ausência em toda a documentação.
- Objetos cobrem páginas de GameObject, primitivas, prefabs, terreno, sprites, tilemap e Canvas; não todos os objetos possíveis de scripts ou plugins.
- A documentação versionada pode reutilizar capturas de versões anteriores.
- Capturas complementares da documentação Unity 2022.3 e 2019.4 são identificadas por imagem; não comprovam a aparência atual da Unity 6.
- Endereços HTTP 404/410 foram excluídos da visualização. Algumas verificações HEAD tiveram HTTP 429 (limite temporário de requisições); o carregamento da galeria depende da disponibilidade do servidor oficial.

Não é um pacote de fotos de todos os tipos. As fichas sem imagem continuam consultáveis, com a origem do tipo. Não foram geradas imagens por IA nem realizadas capturas de uma instalação local da Unity.

## Cobertura por pacote

| Pacote | Versão no inventário | Componentes | Com imagens |
|---|---|---:|---:|
| UnityEngine | 6000.0 | 117 | 78 |
| com.unity.2d.animation | 10.0.3 | 8 | 1 |
| com.unity.2d.spriteshape | 10.0.7 | 2 | 1 |
| com.unity.2d.tilemap.extras | 4.0.2 | 1 | 0 |
| com.unity.addressables | 2.3.16 | 1 | 0 |
| com.unity.ai.navigation | 2.0.5 | 4 | 0 |
| com.unity.animation.rigging | 1.3.0 | 17 | 12 |
| com.unity.cinemachine | 3.1.3 | 77 | 21 |
| com.unity.inputsystem | 1.11.2 | 11 | 2 |
| com.unity.localization | 1.5.13 | 10 | 0 |
| com.unity.netcode.gameobjects | 2.2.0 | 18 | 0 |
| com.unity.splines | 2.7.2 | 5 | 0 |
| com.unity.timeline | 1.8.13 | 1 | 0 |
| com.unity.xr.arfoundation | 6.0.8 | 45 | 11 |
| com.unity.xr.core-utils | 2.4.0 | 2 | 1 |
| com.unity.xr.interaction.toolkit | 3.0.9 | 125 | 45 |
| com.unity.ugui | 2.0 / branch 6000.0 | 54 | 22 |
| com.unity.render-pipelines.core | 17.0.4 | 44 | 0 |
| com.unity.render-pipelines.universal | 17.0.4 | 10 | 0 |
| com.unity.render-pipelines.high-definition | 17.0.4 | 20 | 2 |
| com.unity.visualeffectgraph | 17.0.4 | 3 | 0 |

## Fontes e reprodução

- Imagens e páginas: documentação oficial da Unity, com URL por imagem em [catalogo.json](catalogo.json).
- Universo de componentes: [inventário do projeto](../../componentes/pesquisa-2026-09-15/catalogo-unity.json). A classificação inclui bases/abstratos/obsoletos/ocultos; não é uma lista de 575 itens do menu Add Component.
- Coleta: `python docs/referencias/unity-visual-2026-09-26/coletar.py`; publicação: `python docs/referencias/unity-visual-2026-09-26/publicar.py`.
- As imagens pertencem aos seus respectivos titulares e são apresentadas por referência remota com origem oficial.
