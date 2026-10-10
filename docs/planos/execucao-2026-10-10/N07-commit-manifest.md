# N07 — manifesto da entrega integrada

Lista de caminhos para revisão do commit conjunto N01/N02/N07. Não é comando
de staging nem prova de aceite. Fontes existentes continuam congeladas durante
o build41541. A lista não inclui corpus privado dos plugins ou fontes Synty.
Os arquivos compartilhados devem conservar as alterações N01/N02 da mesma
baseline; não se deve selecionar um cabeçalho antigo para separar assuntos.

## Formatos, receitas e catálogo

```text
native/third_party/ufbx/ufbx.c
native/third_party/ufbx/ufbx.h
native/third_party/ufbx/LICENSE
native/third_party/ufbx/VERSION.txt
native/resources/fbx_import.h
native/resources/fbx_import.cpp
native/resources/fbx_scene_import.h
native/resources/fbx_scene_import.cpp
native/resources/fbx_deformation.h
native/resources/fbx_deformation.cpp
native/resources/fbx_morph.h
native/resources/fbx_morph.cpp
native/resources/unity_yaml.h
native/resources/unity_yaml.cpp
native/resources/unity_subassets.h
native/resources/unity_subassets.cpp
native/resources/unity_animation_import.h
native/resources/unity_animation_import.cpp
native/resources/unity_animation_merge.h
native/resources/unity_animation_merge.cpp
native/resources/unity_prefab_import.h
native/resources/unity_prefab_import.cpp
native/resources/unity_controller_import.h
native/resources/unity_controller_import.cpp
native/resources/unity_import_recipe.h
native/resources/unity_import_recipe.cpp
native/resources/unity_material_import.h
native/resources/unity_material_import.cpp
native/resources/tga_validation.h
native/resources/tiff_decode.h
native/resources/tiff_decode.cpp
native/resources/library_index.h
native/resources/library_index.cpp
native/resources/import_cache.h
native/resources/import_cache.cpp
native/resources/import_node_map.h
native/resources/import_node_map.cpp
native/resources/import_profile.h
native/resources/import_profile.cpp
native/resources/import_report.h
native/resources/import_report.cpp
native/resources/model_import.h
native/resources/model_import.cpp
native/resources/gltf_import.h
native/resources/image_decode.h
native/resources/image_decode.cpp
native/resources/texture_asset.h
native/resources/texture_asset.cpp
native/resources/texture_profile.h
native/resources/texture_profile.cpp
native/runtime/unity_prefab.h
native/runtime/unity_prefab.cpp
```

## Editor, publicação e preview

```text
native/editor/editor_library.h
native/editor/editor_library.cpp
native/editor/editor_unity_import.cpp
native/editor/editor_unity_materials.cpp
native/editor/editor_texture_publication.cpp
native/editor/editor_texture_import_screen.h
native/editor/editor_texture_import_screen.cpp
native/editor/editor_texture_assets.cpp
native/editor/editor_texture_preview.h
native/editor/editor_texture_preview.cpp
native/editor/editor_texture_history.h
native/editor/editor_model_publication.cpp
native/editor/editor_model_takes.cpp
native/editor/editor_fbx_rig_binding.h
native/editor/editor_fbx_rig_binding.cpp
native/editor/editor_import_reconcile.h
native/editor/editor_import_reconcile.cpp
native/editor/editor_import_transaction.h
native/editor/editor_material_object_preview.h
native/editor/editor_material_standalone_preview.h
native/editor/editor_camera_preview.h
native/editor/editor_history.h
native/editor/editor_history.cpp
native/renderer/render_view.h
native/renderer/material_preview_geometry.h
native/renderer/material_override.h
native/renderer/authoring_library_plan.h
native/platform/android/material_preview_resources.h
native/platform/android/material_preview_resources.cpp
native/platform/android/instanced_camera_preview.inl
native/rhi/shaders/material_preview.vert
native/rhi/shaders/material_preview.frag
native/rhi/shaders/material_preview_spirv.h
```

## Arquivos compartilhados necessários

Conferir o diff conjunto nestes caminhos: já existiam e algumas regiões são de
outros blocos. A lista indica dependência da entrega, não afirma que cada
arquivo inteiro foi escrito por N07.

```text
native/CMakeLists.txt
native/editor/editor_session.h
native/editor/editor_session.cpp
native/editor/editor_screen.h
native/editor/editor_screen.cpp
native/platform/android/android_main.cpp
native/platform/android/android_model_picker.h
native/platform/android/android_model_picker.cpp
native/platform/android/instanced_renderer.h
native/platform/android/instanced_renderer.cpp
android/app/src/main/java/dev/aether/editor/ModelPicker.java
native/ui/ui_icon_id.h
native/ui/ui_icon_id.cpp
assets/astra-visual/icons/named/catalog.json
assets/astra-visual/ui/astra-ui-icons.aeui
assets/astra-visual/ui/astra-ui-icons.png
```

## Cenários e ferramentas reproduzíveis

Fixtures FBX/Unity/TIFF abaixo são produzidas pelo projeto e independentes dos
payloads privados. O script de aceite habilita corpus externo por caminho,
sem incorporar as fontes ao repositório.

```text
tests/native/fbx_fixture.h
tests/native/unity_fixture.h
tests/native/tiff_fixture.h
tests/native/test_unity_material_import.cpp
tests/native/test_library_index.cpp
tests/native/test_editor_library.cpp
tests/native/test_editor_library_texture.cpp
tests/native/test_editor_library_fbx.cpp
tests/native/test_editor_library_unity.cpp
tests/native/test_tga_validation.cpp
tests/native/test_tiff_decode.cpp
tests/native/test_fbx_import.cpp
tests/native/test_editor_import_metadata.cpp
tests/native/test_material_preview.cpp
tools/validation/inspect_fbx_sources.cpp
tools/validation/inventory_plugin_sources.py
tools/validation/accept-authoring-library.ps1
tools/cook-material-preview.py
tools/generate-material-preview-icon.py
tools/generate-source-package-icon.py
tools/generate-texture-source-policy-icon.py
tools/generate-rig-binding-icon.py
tools/generate-source-conflict-icon.py
assets/astra-visual/icons/named/materials/object-preview.png
assets/astra-visual/icons/named/assets/source-package.png
assets/astra-visual/icons/named/assets/rig-binding.png
assets/astra-visual/icons/named/assets/source-conflict.png
assets/astra-visual/icons/named/textures/source-policy.png
```

## Documentação e hipóteses visuais próprias

```text
docs/planos/execucao-2026-10-10/README.md
docs/planos/execucao-2026-10-10/plugins-cobertura.md
docs/planos/execucao-2026-10-10/plugins-itens.csv
docs/planos/execucao-2026-10-10/plugins-declaracoes.csv
docs/planos/execucao-2026-10-10/N07-materiais-biblioteca.md
docs/planos/execucao-2026-10-10/N07-biblioteca-indice.md
docs/planos/execucao-2026-10-10/N07-formatos-proximo.md
docs/planos/execucao-2026-10-10/N07-proximo-pacote.md
docs/planos/execucao-2026-10-10/N07-tiff-rig-proximo.md
docs/planos/execucao-2026-10-10/N07-fbx-corpus-deformacao.md
docs/planos/execucao-2026-10-10/N07-unity-pacote.md
docs/planos/execucao-2026-10-10/N07-saldo-real.md
docs/planos/execucao-2026-10-10/N07-commit-manifest.md
docs/planos/execucao-2026-10-10/N07-material-preview-conceito.png
docs/planos/execucao-2026-10-10/N07-textura-importador-conceito.png
docs/planos/execucao-2026-10-10/N07-unity-conflito-conceito.png
```

Logs/capturas de aceite são selecionados somente após a rodada vigente, mantendo
o ledger e distinguindo UI CPU, publicação simulada/real e Vulkan. Não incluir
diretórios temporários completos, imagens de fontes privadas ou dados sensíveis.
Publicação AstraDocs é coordenada pela raiz em outro checkout.

## Dependência N03 já incluída no CMake atual

Estes arquivos **não são os novos módulos isolados**. O CMake já os referencia
e eles foram compilados na baseline anterior. Devem acompanhar o CMake ou ter
commit próprio anterior; omiti-los produziria checkout que não configura.
A compilação não encerra N03, cujo editor/placement/aceite permanecem pendentes.

```text
native/resources/rig_asset.h
native/resources/rig_asset.cpp
native/resources/rig_math.cpp
native/resources/rig_evaluation.h
native/resources/rig_evaluation.cpp
native/runtime/rig_instance.h
native/runtime/rig_instance.cpp
tests/native/test_rig_authoring.cpp
```

## Excluir desta entrega integrada

Preparação N03 fora do CMake: `editor_rig_authoring_candidate.*`,
`editor_rig_overlay.*`, `editor_rig_preview.*`, `runtime/rig_placement.h`,
`editor_rig_screen.*`,
`runtime/rig_placement_binding.*`, `scene/rig_instance.h`,
`platform/android/preview_thumbnail_readback.*`,
`tools/generate-rig-authoring-icons.py` e documentos/conceito N03.
Também excluir N08 isolado, corpus `Downloads/analise-plugins`, arquivos
`poly.unitypackage`, extrações privadas, fontes/licenças comerciais copiadas
para testes locais, `build/`, caches, executáveis e temporários do compilador.
Licença do ufbx listado acima acompanha o código permissivo incorporado.
