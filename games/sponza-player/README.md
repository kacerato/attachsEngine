# Sponza: player e iluminação, 2026-10-05

Patch autoral para o projeto Sponza existente no POCO F7. Não é uma cópia completa dos arquivos da Intel Sponza: as fontes e texturas permanecem no projeto original. Os quatro arquivos em `patch/` usam os serializers nativos da Astra e as identidades existentes da importação.

A cena original tem **3.738.920 triângulos visuais**. A cápsula visual do player acrescenta 1.024, totalizando **3.739.944**. A contagem de primitivas disponíveis na biblioteca de recursos é diferente da contagem de objetos instanciados na cena. Triângulos visíveis dependem da câmera.

O objeto raiz da importação tinha um MeshRenderer vazio e um MeshCollider sem fonte. Removemos esses componentes e o corpo estático vazio desse grupo. As malhas originais continuam nos filhos. A arquitetura do primeiro e segundo pisos recebeu 70 corpos estáticos e 227 formas de colisão: pisos, paredes e tetos usam a geometria original; detalhes usam 219 receitas de colisão derivada, com objetivo de 5% dos triângulos e erro relativo máximo 0,01. O resultado físico tem 393.416 triângulos. Luminárias, decals e detalhes do terceiro piso não recebem colisores. A simplificação física não muda a geometria visual nem seu LOD.

`Player Sponza` (160) usa o Character nativo, cápsula de raio 0,32 m, velocidade de 3,8 m/s, gravidade e salto. `Corpo do player` (161) usa a cápsula visual existente. `Camera do player` (162) fica a 1,65 m, é filha do player e usa CameraLook. `Controles do player` (163) referencia `UI/SponzaPlayer.aeui`: joystick Mover, área Olhar e botão Saltar, ligados ao receptor e à câmera por referências de objeto persistentes. Não há script novo nem controlador paralelo.

O sol reutiliza a luz existente (158), agora direcional, com intensidade 8 nas unidades legadas, temperatura 5.600 K e ângulo de 82°/−22°. Seis posições de luminárias importadas recebem luz pontual de 3.000 K, 30.000 lm e alcance 7 m. O ambiente existente foi ajustado para luz indireta neutra, exposição +0,20 EV e bloom discreto. Sombras locais permanecem desligadas nessas seis fontes; a sombra solar usa o sistema de cascatas existente. A primeira proposta ficou azul e escura nas capturas, e foi refinada antes da entrega.

## Reproduzir a autoria no host

```powershell
cmake --build build/editor-host --target sponza_scene_author -j 4
./build/editor-host/sponza_scene_author.exe <backup-original-do-projeto-Sponza> <pasta-de-saida-separada>
```

O programa em `tools/sponza_scene_author.cpp` lê a cena, o glTF/bin e o mapa/perfil de importação reais. Valida as colisões com Jolt, os controles com SceneGui, caminhada, salto e câmera, e o round-trip da cena antes de escrever a saída. A leitura de geometria para validação no host dispensa as texturas; essa saída não substitui o cache de renderização do aparelho. É uma ferramenta específica desta revisão da Sponza, não um importador genérico. Não a aplique sobre uma cena já modificada.

## Referências

- [Godot 4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html): cápsula, locomoção, slope e floor snap pertencem ao personagem; a colisão estática pertence ao cenário. Na Astra, reutilizamos Character e ScenePhysics existentes.
- [Godot 4.5 Collision shapes](https://docs.godotengine.org/en/4.5/tutorials/physics/collision_shapes_3d.html): malhas côncavas são formas de cenário estático; derivados físicos podem ser separados das malhas visuais. Aplicação: Collider com `collisionMesh` persistente e receitas do ImportProfile existente.
- [Godot 4.5 Environment e post-processing](https://docs.godotengine.org/en/4.5/tutorials/3d/environment_and_post_processing.html): separar ambiente, luz direta e exposição. Aplicação: ajustar os componentes Environment/Light já executados pela Astra.
- [Proposta oficial de joystick da Godot](https://github.com/godotengine/godot-proposals/issues/11192): discute controle virtual e o workflow no editor Android; é uma proposta, não evidência de suporte lançado na Godot. A Astra usa seu próprio GuiDocument/SceneGui já implementado.
- [Unity 6000.3, Introduction to Dynamic Resolution](https://docs.unity.com/en-us/engine/6000.3/manual/cameras/resolution-scale/dynamic-resolution/introduction): a área desenhada pode diminuir dentro de uma textura cuja alocação permanece fixa. A validação desta cena encontrou uma divergência entre essas extensões quando a política térmica reduzia sua escala máxima; a correção Android mantém a extensão dos recursos vinculada à política de criação e usa a escala ativa para o desenho e a reconstrução.

O aceite e as capturas reais ficam em `docs/validacao/sponza-player-2026-10-05/`. O backup completo do projeto anterior fica em `build/sponza-player-20261005/original/Sponza/`, fora do versionamento. Para reverter a autoria, restaurar a cena, o registro de recursos e o perfil originais com o aplicativo fechado. O UI novo pode permanecer sem referência; não é necessário apagar fontes ou caches.
