# Backrooms — três projetos independentes Astra

## Jogos e protocolos

| Projeto | Mecânica | Direção gráfica |
|---|---|---|
| Nível 0 — Circuito Amarelo | Recolher três fusíveis por proximidade; manter Operar por 2 segundos junto aos painéis correspondentes; restaurar circuitos; extrair | Papel de parede deteriorado e carpete fotografados, teto baixo, ilhas de paredes e voltas, lâmpadas quentes |
| Reservatório 04 | Válvulas na ordem 2 → 1 → 3; ordem errada reinicia os controles; válvulas e saída usam tween nativo | Vitrine Ultra: azulejos, concreto e metal PBR, tanques, tubulação GLB importada, point/spot com sombras, volume de ambiente local, SSAO, bloom, AgX, TAA, vinheta, grão e exposição limitada |
| Subsolo — Último Turno | Painéis 1 → 2 → 3; primeiro inicia 180 segundos; blecautes de 5 segundos a cada 18; atravessar saída antes do prazo | Corredores técnicos com concreto e metal gastos, estantes, fontes de alcance curto e interrupções da rede |

Todos começam sem energia nos circuitos, com a lanterna ligada. Sem energia e sem lanterna, não deve existir luz ambiente artificial. Após restaurar um setor, as luminárias daquele setor passam a iluminá-lo de verdade. Restaurar todos não equivale a acender um ambiente global.

Controles: arraste à esquerda para mover, à direita para olhar; Saltar usa o botão próprio; **toque curto em Operar alterna a lanterna**, mantenha por 2 segundos para operar um painel. Tecla F também alterna a lanterna; E opera. Uma retenção iniciada fora do alcance não deve concluir um painel ao entrar nele.

## Cadeia funcional

`scenes/editor.aescene` v17 → componentes tipados → `BackroomsExpedition` e classe de cada jogo → consumidor nativo de luz/mesh/áudio/personagem/tween → renderer/física/áudio. Os circuitos alteram `Light.Enabled`, intensidade e emissão das luminárias; progresso altera emissão dos segmentos. A porta visual se desloca por tween e seu colisor só é removido quando o tween termina. Falhas e vitória interrompem a interação. Stop/Play reinicia a missão; não há savegame de progresso.

Os projetos contêm `project.json`, cenas, Assets, Scripts, Audio e `.astra/assets.astra`. Objetos e parâmetros são editáveis no editor. `ArtSources` preserva os mapas PBR de preparação. O gerador usa o kit existente, corrigindo winding somente na geometria exportada dos jogos. Placas possuem UV corrigida no GLB da coleção, sem alterar outras coleções.

## Correção necessária no renderer

`native/renderer/punctual_lights.h`: uma direcional autoral **habilitada com intensidade zero** agora suprime o sol padrão. Antes, `selectDirectionalLight` descartava essa fonte e deixava o sol de fallback iluminar interiores. As luzes locais com energia zero continuam sem ocupar vagas. Fontes positivas continuam ganhando a seleção por intensidade, e cenas sem direcional mantêm o comportamento padrão. O teste dirigido está em `authoring/check_dark_directional.cpp`.

Há no máximo oito luzes locais: seis de circuito, uma da saída e uma da lanterna. O sol direcional é separado. Sombras point custam seis faces; spot, uma. O perfil máximo solicita resolução integral e desativa resolução dinâmica; pode custar bastante GPU. Perfis pedidos e efeitos executados são registrados separadamente pela bancada.

## Origem dos recursos e referências

Texturas fotografadas: [dirty_carpet](https://polyhaven.com/a/dirty_carpet), [decrepit_wallpaper](https://polyhaven.com/a/decrepit_wallpaper), [dirty_tiles](https://polyhaven.com/a/dirty_tiles), [yellow_plaster](https://polyhaven.com/a/yellow_plaster), além de concreto, metal e rocha CC0 da coleção existente. Metadados, URLs e hashes estão em `sources.json`; downloads conferem MD5 upstream. [Poly Haven: licença CC0](https://docs.polyhaven.com/en/faq). Tubulação: `modular_industrial_pipes_01`, CC0, já disponível na coleção local.

Referências funcionais, **Godot 4.5**:

- [Light3D: cor, energia, alcance e sombra](https://docs.godotengine.org/en/4.5/classes/class_light3d.html): energia e sombra pertencem à fonte; apagar uma fonte deve apagar sua contribuição, sem ressuscitar outra fonte.
- [Código oficial Light3D, 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/light_3d.cpp): atualização dos parâmetros do recurso consumido pelo renderer.
- [Luzes e sombras: criação, propriedades e uso](https://docs.godotengine.org/en/4.5/tutorials/3d/lights_and_shadows.html): fontes locais de alcance controlado, custo separado de point/spot e sombras, edição no Inspector. A coleção adapta esses princípios ao orçamento de oito luzes da Astra.
- [Arquitetura de renderização](https://docs.godotengine.org/en/4.5/engine_details/architecture/internal_rendering_architecture.html): separação entre luz direta, iluminação global e volumetria.

Pesquisei packs de Backrooms importáveis. O kit de 3DAssets possui arquitetura CC0, mas declara modelos gerados por IA; não foi usado para substituir o trabalho de realismo. O pack de NaiveGoblin também foi localizado, mas não foi baixado/importado. **Os mapas principais são autorais; não são um mapa externo importado.**

## Limites reais

Não há VoxelGI, neblina volumétrica, luz de área, cookies, SSR ou captura de reflection probes local nesta entrega. A neblina existente é por profundidade e foi configurada com cor preta para não revelar o interior no breu. Materiais têm reflexos diretos das luzes, mas não simulam reflexos corretos de todo o cômodo. Lightmap externo existe em parte do renderer, mas esta coleção não inclui bake nem declara GI integrada.

O perfil Ultra é uma bancada gráfica; sua existência não prova fluidez nem realismo visual final. Contagem gerada: 133 entidades no Nível 0, 178 no Reservatório e 147 no Subsolo. Dois tipos concretos de script por projeto incluem o jogo e a bancada LightProbe, que não está ligada ao mapa jogável.

## Reprodução e aceite

```powershell
python games/backrooms/authoring/fetch_assets.py
python games/backrooms/authoring/build_projects.py
python games/backrooms/authoring/device.py deploy nivel-0
python games/backrooms/authoring/device.py deploy reservatorio-04
python games/backrooms/authoring/device.py deploy ultimo-turno
python games/backrooms/authoring/device.py launch reservatorio-04
python games/backrooms/authoring/device.py probe-run reservatorio-04
```

A bancada cria uma cópia **Reservatório Lighting Lab** no aparelho e exercita cinco estados na mesma vista: spots → tudo apagado → só lanterna → spots restaurados → só direcional. Cada estágio dura 8 segundos. Capturas reais, logs e medidas de luminância sRGB do viewport ficam em `evidence`. A medida verifica escuridão na imagem; não é medida fotométrica nem prova de GI.

Aceite jogável ainda exige: percorrer cada mapa com colisão; demonstrar cancelar o hold; executar cada protocolo; verificar iluminação do setor; conferir saída visual e física; concluir e reiniciar. A bancada de luz não prova esses passos automaticamente. Veja `evidence/VALIDACAO.md` para o que foi efetivamente observado.
