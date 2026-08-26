# ADR-03 — Vulkan 1.3 como único backend gráfico

- **Estado:** aceita; implementação parcial (shell mínimo validado em 1 GPU física)
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 2.1
- **Decisores:** arquitetura, renderer

## Contexto

O renderizador precisa de controle fino de memória e bindless para o pipeline
GPU-driven planejado (§5 do plano). As alternativas de portabilidade (OpenGL
ES, WebGPU) não expõem o controle necessário; Metal nativo exigiria um
segundo backend completo para iOS.

## Decisão

Vulkan 1.3 é o único backend gráfico da engine. iOS é coberto via MoltenVK
(tradução Vulkan→Metal), não por um backend Metal nativo separado.

## Alternativas descartadas

1. **OpenGL ES.** Sem bindless (`descriptor_indexing`) nem controle de
   memória necessário para GPU-driven rendering — teto de desempenho baixo
   demais para o pipeline planejado.
2. **WebGPU.** Camada de abstração adicional sobre o que já seria
   implementado em Vulkan — indireção sem ganho, no estágio atual do
   projeto.
3. **Metal nativo no iOS (backend separado).** Dobraria a superfície de
   manutenção do RHI (dois backends completos) por uma plataforma; MoltenVK
   cobre o caso com custo de manutenção muito menor.

## Evidência

- `native/rhi/device.cpp`, `native/rhi/device.h`: inicialização de
  instância, seleção de dispositivo, filas, swapchain.
- `native/rhi/device_profile.h/.cpp`: perfis de dispositivo S/A/B/C,
  detectando capabilities (bindless, ray query, mesh shader, VRS) em vez de
  presumi-las — testado headless em `tests/native/test_device_profile.cpp`
  (8 testes).
- `native/platform/android/android_vulkan_surface.*`,
  `native/platform/android/android_window.*`: shell Android.
- **Validado em hardware físico real** (Xiaomi SM8735/Adreno): instância,
  device, surface, swapchain, pipeline e apresentação de um triângulo RGB
  confirmados por log real de dispositivo; 100 ciclos de
  background/foreground, screen off/on e mudança de configuração sem crash
  (`docs/ESTADO.md`, "Shell Android — validação atual").

## Divergência registrada

`native/rhi/device.cpp` cria a instância Vulkan pedindo `apiVersion =
VK_API_VERSION_1_1`, com o comentário explícito de que "Vulkan 1.1 é o piso
de hardware definido pelo plano. Recursos 1.3 são habilitados por capability
profile, nunca presumidos na criação base." Já `native/rhi/device_profile.h`
comenta "Núcleo Vulkan 1.3 (linha de base assumida pelo plano)" e expõe um
campo booleano `vulkan1_3` avaliado via capability query.

Isso não é uma violação da decisão — é a tradução prática dela: "Vulkan 1.3
como único backend" significa que a engine não terá um segundo backend
gráfico (OpenGL/Metal nativo), não que todo aparelho-alvo precisa reportar
1.3 na criação de instância. Detectar 1.3 como capability, com fallback de
perfil, é o que permite rodar em hardware que só oferece 1.1/1.2 sem excluí-lo
da matriz de suporte — mas a nomenclatura da ADR ("Vulkan 1.3") e o código
("pede 1.1, detecta 1.3") merecem ficar registrados lado a lado para não
confundir leitura futura.

## Limitação atual

Apenas 1 de ~6 GPUs da matriz mínima de hardware foi validada (Adreno);
faltam Mali, PowerVR, Apple e um perfil C fraco (`GAP-HW-01`, aberta —
`docs/PLANO-FECHAMENTO-LACUNAS.md`). Nenhuma cena real roda no pipeline
gráfico direto ainda — apenas RHI básico e Render Graph headless existem
(ver ADR-04). O Gate M2 do plano principal permanece fechado.

## Consequências

- Todo trabalho de pipeline gráfico direto (depth prepass, PBR, sombras,
  IBL, pós-processamento — itens 2.4.x) deve assumir bindless via
  `descriptor_indexing` como padrão de acesso a textura, não descriptor set
  por objeto.
- Qualquer feature que dependa de uma extensão opcional (`ray_query`,
  `mesh_shader`, `fragment_shading_rate`) deve ser gated por
  `DeviceProfile`, nunca presumida disponível.
