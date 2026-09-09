# Interface original Godot no Astra

**Estado atual:** por solicitação do usuário, a abertura dos projetos voltou para `AetherActivity`, o editor nativo anterior. O host Godot não é mais aberto pelo shell. As notas abaixo registram o experimento anterior.

## Etapa autorizada: interface primeiro

Em 09/09/2026 o usuário priorizou incorporar a interface original e conectar a lógica Aether depois. O aplicativo mantém o launcher `AstraShellActivity`, a criação e o índice de projetos. Abrir um projeto agora inicia `GodotEditorActivity`, no processo `:editor_ui` do mesmo APK.

A biblioteca oficial fixada em `upstream.json` fornece `SceneTreeDock`, `CreateDialog`, `EditorInspector`, `FileSystemDock` e os demais controles originais. Não são redesenhados pelo `editor_screen.cpp` Aether. A camada Android da biblioteca é compilada do fonte; sua biblioteca nativa vem do APK oficial verificado por SHA512. Licença MIT: `LICENSE-Godot.txt`.

`GodotEditorProject` cria somente `project.godot` se não existir. Não cria nós, cenas de exemplo ou uma câmera automaticamente. Mantém os arquivos `.aescene` e o índice de projetos. Cenas criadas na interface original usam `.tscn`; ainda não existe conversão ou sincronização para Aether.

A escala inicial usa o ajuste oficial do editor: personalizada em 150%, independente do DPI Android. Pode ser alterada nas configurações originais depois. Os PNGs Astra disponíveis sobrescrevem os ícones principais pelo recurso Theme; os demais ícones continuam upstream nesta etapa. O layout e os widgets permanecem originais.

## Limite explícito

O runtime, renderer Vulkan e módulos gerenciados Aether continuam no APK e no repositório. **Eles ainda não estão conectados ao viewport ou aos nós desta interface.** O viewport apresentado pelo host é o viewport de ferramentas Godot, não evidência de renderização Aether. A execução de outro processo Godot é recusada pelo host com uma mensagem de indisponibilidade; não substitui Play Aether silenciosamente.

Próxima integração: adaptar nós/propriedades à identidade e comandos `EditorSession`, publicar a superfície Aether no viewport e conectar Play/Stop ao runtime Aether. A fase atual não declara equivalência funcional de runtime.

## Build

O Gradle principal executa `tools/prepare-godot-ui.py` quando o AAR está ausente ou suas entradas mudam. O preparador valida a revisão e o APK oficial, compila a camada Android e gera `build/godot-ui/godot-editor-ui.aar`. Não importa o aplicativo completo nem troca o launcher. O AAR não duplica libc++; os símbolos C++ requeridos foram comparados com o NDK Aether 27.1.

`tools/verify-aether-apk.py` exige entrada Astra, bibliotecas Aether, módulos gerenciados e biblioteca Godot de interface. Debug e Release usam o mesmo host. `tools/build-godot-editor.ps1` continua sendo apenas o laboratório separado `dev.aether.godotlab`.

## Lifecycle e persistência

O processo separado impede que o encerramento/reinício Godot encerre o processo do shell Aether. Configurações da interface usam arquivos atômicos e são inicializadas uma vez; preferências posteriores são preservadas. Arquivos existentes `project.godot` não são sobrescritos. Os projetos Aether não são convertidos nem removidos.

Não interpretar a presença dos controles originais como conclusão da ponte de execução.
