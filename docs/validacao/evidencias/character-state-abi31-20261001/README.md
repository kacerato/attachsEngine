# Estado do personagem — ABI31

Host: dois cenários novos passaram, usando Jolt real (motor/parede/apoio/salto e ABI/Start/inatividade/Stop). Regressões: reconstrução 4/4, chão 4/4, schemas 13/13, atlas 5/5. SDK: 54/54; 16 fixtures compiladas; layout gerenciado de 80 bytes conferido. Logs acompanham este arquivo.

`ground-portrait.png` e `air-landscape.png` são capturas do Inspector executável rasterizado em host, com consulta real de ScenePhysics. Inspecionadas após remover duplicação: uma leitura de apoio e uma de velocidade; amostra de salto Y=5 m/s. Em landscape 853×394, propriedades inferiores usam scroll existente. Viewport vazio não é prova de renderização da cápsula; não há captura Vulkan/aparelho deste pacote.

Build Android final passou; SDK e atlas do APK correspondem aos artefatos gerados. Tentativa de instalar falhou por ausência de transporte ADB; `adb devices -l` retornou lista vazia. Projeto `build/acceptance/CharacterState-20261001` está pronto. Manifesto registra instalação e aceite físico como falsos. Pacote tem implementação e validação host; aceite Android permanece pendente.
