# F077 — up e roll autorados no caminho

Este bloco fecha a lacuna de orientação do Curve3D local consumido pelo Path. O valor é propriedade tipada do componente da cena, sem fabricar um asset externo ou GUID que não tenha consumidor. Autoria usa os IDs persistentes dos pontos; caches e frames são derivados. A conclusão exige os aceites do registro, não este documento sozinho.

## Contrato e consumidor

Cada ponto mantém posição, handles de entrada/saída e acrescenta roll em graus [-3600,3600], contínuo e sem reduzir módulo 360. A curva tem up inicial local (default 0,1,0), finito, limitado por canal e não nulo. Editar roll não troca ID, ordem ou fronteira de alocação. Path v2 persiste os novos dados com max_digits10; leitura v1 conserva IDs, contador, geometria e fechamento, acrescentando os defaults explicitamente. Arquivos inválidos não publicam parte da curva.

O bake adaptativo existente continua limitado a 128 pontos, profundidade 12 e 32768 amostras. Calcula geometria/comprimento, interpola roll por distância dentro de cada trecho entre pontos e constrói frames por transporte paralelo. O up inicial é projetado no plano perpendicular à tangente. Se for paralelo, a convenção explícita escolhe o eixo positivo menos alinhado; vetor up zero continua sendo erro. Em uma cúspide de 180°, preserva up e inverte right: a geometria não determina uma rotação única nesse ponto. Tangentes nulas locais tentam as arestas adjacentes; curva totalmente degenerada tem comprimento zero e não oferece frame orientado.

Curva fechada usa a mesma tangente na emenda e distribui a diferença geométrica de orientação por distância para alinhar os frames inicial/final. Aplica roll depois, em torno de +Z/tangente. sampleFrame publica posição, tangente, up, right e roll só após sucesso. Não transforma um erro em frame identidade. sampleDistance continua oferecendo posição/tangente, inclusive o caso explicitamente degenerado anterior.

ScenePaths transforma geometria e up local pelo transform mundial antes de bake. Assim, posição, comprimento, distância, frame e offsets usam o mesmo espaço. Edição de up, roll, pontos, handles, fechamento ou transform invalida o cache; falhas também são cacheadas. O seguidor usa o frame real tanto para orientar sua frente +Z quanto para aplicar offset lateral/up/frontal. Em reverso, inverte tangente e right, mantendo o up autorado. Autoridade da física, escritores concorrentes, dependências e lifecycle continuam passando pela cadeia real de GameWorld. Estado do editor não dirige a pose.

## SDK e ABI35

O layout de ScriptSceneAccess permanece igual, mas a versão passa a 35 pelo protocolo ampliado. NativeBehaviorRuntime rejeita a versão anterior em vez de escrever dez floats em um backend que só conhecia nove. O SDK mantém os comandos antigos: leitura 1/2 tem nove floats; inserção 3 recebe nove/default roll zero; edição 4 recebe nove e preserva roll existente; remoção/movimento 5/6 conservam IDs. Comandos 7/8 leem dez floats por índice/ID; 9/10 inserem/editam posição+handles+roll atomicamente. Runtime 5 devolve posição/tangente/up/roll (dez floats) e comprimento; runtime 0 continua a amostra de seis floats.

CurvePath.At/ById retornam RollDegrees; Insert aceita roll opcional; Set sem roll preserva o valor, e Set com roll publica o ponto inteiro. Up usa SetTriple existente, com três canais validados/publicados atomicamente. SampleFrame retorna PathFrame, com Right derivado de Up×Tangent. Buffers usam stackalloc. O facade PathComponent é gerado dos descritores; não se mantém uma segunda lista manual de campos.

## Autoria e inspeção

NÃO IREI SER SIMPLISTA NO DESIGN.

Pontos usa três destinos: Posição, Tangentes e Orientação. Orientação mostra roll do ponto selecionado e up inicial da curva. A seleção continua sendo o ID durável, inclusive durante teclado, reordenação e histórico; resultado de teclado obsoleto é recusado. As ações estruturais ficam nos destinos de geometria para reservar espaço útil à orientação em paisagem 853×394. O campo global up também funciona quando não há pontos. Zero up é recusado pelo mesmo modelo, com o feedback numérico existente. Undo/Redo e save/reopen usam o histórico/archive reais.

A captura anterior orientou uma hipótese gerada; ela não é evidência de implementação. O novo ícone path/orientation tem SVG/raster próprios, catálogo e enum/atlas reais. Pequenas setas de frame no viewport vêm do mesmo bake e roll, com quantidade limitada; elas não são um desenho hardcoded do conceito. Captura posterior e interação executável determinam o aceite. Raster host não comprova Vulkan Android.

## Referências e adaptação

- [Godot 4.5 Curve3D](https://docs.godotengine.org/en/4.5/classes/class_curve3d.html): tilt autorado e cache de orientação separado da geometria.
- [Fonte Godot 4.5-stable Curve3D::_bake](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/curve.cpp): transporte paralelo e correção da emenda; extraímos esses princípios para o bake Astra. A Astra interpola roll por distância por trecho e usa graus; Godot usa radians e amostras de parâmetros. Não há promessa de solver idêntico.
- [Godot 4.5 PathFollow3D](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html): orientação/tilt como consumidores do recurso, separados do offset ao longo do caminho. A Astra mantém referência explícita e +Z como frente.
- [Proposta Godot #8890](https://github.com/godotengine/godot-proposals/issues/8890), relato do workflow de edição: alças de tilt podem competir com seleção de pontos/tangentes. Adaptamos destinos contextuais e seleção estável para touch. O source do plugin e vídeos não ficaram acessíveis pelo provedor; não se atribuem observações de frames/cliques a conteúdo não inspecionado.

## Aceite focado

O alvo aether_spline_frame_family_tests reutiliza os cenários de geometria, identidade, histórico, input e Play/ABI; amplia a ponte real para o ponto completo e frame, substitui a expectativa antiga de falha em trechos verticais e acrescenta três cenários de matemática/migração, input de orientação e pose/offset com edição real. O C# mede separadamente o transporte SDK com recorder explícito; esse recorder não prova o consumidor nativo. Evidências finais, capturas e pacote ficam em docs/validacao/evidencias/families-spline-frame-20261001/. Não houve execução física desta revisão.
