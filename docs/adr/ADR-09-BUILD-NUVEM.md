# ADR-09 — Build nativo em nuvem, dados no dispositivo

- **Estado:** aceita como decisão de design; **não implementada**
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 8.2
- **Decisores:** arquitetura, plataforma/build

## Contexto

Compilar um APK/AAB/IPA final exige toolchain completa (NDK, Gradle,
assinatura, empacotamento) — inviável de rodar inteiramente dentro de um
celular hoje, tanto por recursos de CPU/memória quanto pela ausência de
toolchains oficiais para celular. Ao mesmo tempo, os dados do projeto
(assets, cenas, scripts) devem permanecer sob controle do usuário, não
presos a um servidor.

## Decisão

O build nativo final (empacotamento de plataforma) roda em um serviço de
nuvem; os dados do projeto continuam residindo no dispositivo do usuário
(ou em um repositório git que ele controla). O "modo Player" (executar o
jogo direto via interpretador/hot reload, sem empacotar) cobre a grande
maioria das necessidades de iteração sem depender da nuvem.

## Alternativas descartadas

1. **Toolchain completa no celular.** Impossível hoje: NDK/Gradle/toolchain
   de assinatura não têm porte viável para Android/iOS rodando dentro de si
   mesmos, e o custo de engenharia para viabilizar isso não se justifica
   frente ao modo Player cobrir ~95% do ciclo de iteração.

## Estado de implementação

**Nenhuma implementação existe como feature de produto.** `docs/MATRIZ-MARCOS.md`
(Etapa 8.2 — Build farm em nuvem) marca os itens 8.2.1–8.2.4 e 8.2.6 como
"não iniciado"; o item 8.2.5 é "parcial" apenas porque um APK debug/release
ARM64 já é gerado **localmente** via Gradle (evidência em `docs/ESTADO.md`),
não através de nenhuma build farm em nuvem.

O CI existente (`.github/workflows/ci.yml`, `android-device.yml`) é
infraestrutura de **desenvolvimento** (testes/verificação do próprio
repositório da engine), não a feature de produto "usuário final builda seu
jogo na nuvem". `docs/ESTADO.md` confirma que nem essa infraestrutura de CI
rodou hospedada ainda — só reproduzida localmente, pois o repositório ainda
não tem remote publicado.

## Consequências

- Esta ADR existe para registrar a decisão antes da Etapa 8.2 começar —
  quando o build farm entrar em desenvolvimento, a arquitetura (dados no
  dispositivo/git do usuário, build final em serviço remoto) já está
  decidida.
- O modo Player (interpretador/hot reload local) precisa continuar sendo o
  caminho primário de iteração enquanto o build em nuvem não existe — não
  há hoje nenhum caminho de "gerar um binário instalável" fora do Gradle
  local que um desenvolvedor da própria engine já usa.
