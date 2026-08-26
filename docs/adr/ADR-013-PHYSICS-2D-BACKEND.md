# ADR-013 — Backend de física 2D

- **Estado:** proposto; aguardando perfis móveis B e C
- **Data:** 26/08/2026
- **Lacuna:** `GAP-2D-01`
- **Decisores:** arquitetura, física, Android e performance

## Contexto

O runtime já usa Jolt para física 3D e expõe `AetherAllowedDOFs::Plane2D` para
restringir corpos ao plano XY. Essa restrição entrega comportamento 2D sem um
segundo mundo, mas não reduz o custo do solver 3D. O item 4.1.6 exigia comparar
essa solução com Box2D v3 antes de tornar a escolha definitiva; a comparação
não existia.

Box2D v3.1.1 foi vendorizado, com commit e licença registrados em
`native/third_party/Box2D/VENDORED_COMMIT.txt`. Ele só participa dos alvos de
benchmark e não alterou a API nem o runtime da engine.

## Alternativas

1. **Manter somente Jolt restrito.** Um backend, um lifecycle e possibilidade
   de interação 2D/3D no mesmo mundo; paga custo de solver e memória 3D.
2. **Adicionar Box2D como backend 2D.** Melhor especialização e potencial de
   CPU/memória, mas adiciona biblioteca, mundo, sincronização, testes e casos de
   interoperabilidade.
3. **Substituir Jolt.** Rejeitada: Box2D não cobre a física 3D do produto.
4. **Escrever solver próprio.** Rejeitada pelo ADR-05 e pelo custo/risco.

## Método reproduzível

`tests/native/benchmark_physics2d.cpp` gera um comparador e dois runners
isolados. Todos usam círculos de 0,45 m, piso de 100×1 m, posições equivalentes,
gravidade de 9,81 m/s², `dt=1/60`, três trials, warm-up e
excitação horizontal alternada de ±0,01 m/s para neutralizar diferenças de
dormência. Ambos usam quatro passos internos por frame (`collisionSteps` no
Jolt e `subStepCount` no Box2D), evitando premiar uma configuração rápida que
comprima a pilha além da qualidade equivalente. São coletados média, p50, p95, p99, máximo, RSS, memória interna
diagnóstica, corpos criados, contatos e validade numérica final. Centroide e
altura da pilha precisam concordar entre backends dentro de 10%; o benchmark
falha se o ganho de tempo vier acompanhado de qualidade física divergente.

Execução:

```powershell
tools/run-physics2d-ab.ps1 -Platform Host
tools/run-physics2d-ab.ps1 -Platform Android -NdkRoot <ndk> -DeviceProfile B
tools/run-physics2d-ab.ps1 -Platform Android -NdkRoot <ndk> -DeviceProfile C
```

O script grava saídas parseáveis e `metadata.json` com commit, aparelho,
temperatura inicial/final e tamanho dos binários. O workflow
`android-device.yml` executa o mesmo caminho no runner físico.

## Evidência host inicial

Esta execução é diagnóstico de integridade do harness, não decisão mobile.
Ambos os motores criaram todos os corpos e terminaram estáveis. Até 1.000
corpos, ambos mantiveram p99 abaixo de 16,6 ms; em 5.000, o Jolt chegou a
18,17 ms e o Box2D a 6,04 ms.

| Corpos | Jolt p50 | Box2D p50 | Ganho Box2D | Jolt p95 | Box2D p95 | Ganho Box2D |
|---:|---:|---:|---:|---:|---:|---:|
| 50 | 0,295 ms | 0,012 ms | 95,9% | 0,340 ms | 0,016 ms | 95,3% |
| 100 | 0,439 ms | 0,024 ms | 94,6% | 0,504 ms | 0,027 ms | 94,6% |
| 200 | 0,836 ms | 0,046 ms | 94,5% | 1,350 ms | 0,054 ms | 96,0% |
| 500 | 2,476 ms | 0,209 ms | 91,6% | 2,822 ms | 0,353 ms | 87,5% |
| 1.000 | 4,988 ms | 0,619 ms | 87,6% | 5,857 ms | 1,021 ms | 82,6% |
| 5.000 | 15,026 ms | 3,882 ms | 74,2% | 17,053 ms | 5,776 ms | 66,1% |

Nos runners isolados, o pico de RSS incremental observado em 5.000 corpos foi
10,22 MiB no Jolt e 7,90 MiB no Box2D. Os binários isolados mediram 20,47 MiB e
0,36 MiB, respectivamente. O tamanho do Jolt já é custo existente do runtime;
adotar Box2D acrescentaria aproximadamente o segundo valor, não o substituiria.
`internal_bytes` não entra no gate porque o Jolt informa arena reservada e o
Box2D informa bytes alocados — semânticas diferentes.

## Evidência Android perfil A

O caminho NDK/ARM64/ADB foi executado no Xiaomi 25053PC47G (API 36, Qualcomm),
com bateria de 34,4 °C a 35,0 °C. A geometria permaneceu equivalente e ambos os
backends terminaram estáveis. Nas cargas decisórias:

| Corpos | Jolt p50/p95 | Box2D p50/p95 | Ganho Box2D p50/p95 |
|---:|---:|---:|---:|
| 500 | 6,346 / 7,728 ms | 0,193 / 0,246 ms | 97,0% / 96,8% |
| 1.000 | 10,167 / 13,225 ms | 0,643 / 0,720 ms | 93,7% / 94,6% |
| 5.000 | 28,776 / 34,040 ms | 3,254 / 3,939 ms | 88,7% / 88,4% |

O RSS isolado em 5.000 corpos foi 7,53 MiB no Jolt e 11,52 MiB no Box2D; logo,
CPU e memória não apontam sempre para o mesmo vencedor. O perfil A prova o
pipeline e reforça a vantagem de CPU, mas não satisfaz o gate, deliberadamente
definido para os aparelhos mais limitados B e C.

## Gate da decisão

A decisão só muda para **Box2D** se todos estes critérios forem atendidos:

1. runners isolados verdes nos perfis B e C, sem corpo perdido, NaN ou falha;
2. ganho de pelo menos 30% em p50 **e p95** nas cargas de 500 e 1.000 corpos em
   ambos os perfis, ou redução de RSS isolado de pelo menos 30% nesses perfis;
3. p99 sem regressão e dentro do orçamento de 16,6 ms;
4. tamanho incremental e manutenção do segundo mundo aceitos explicitamente.

Se o gate falhar, a decisão é manter Jolt restrito. Se passar, esta ADR será
marcada `aceita`, mas a implementação do backend e da interface neutra seguirá
o item 4.1.6 do plano principal; o plano de lacunas não antecipará esse roadmap.

## Consequências atuais

- Não há Box2D no runtime, ABI C#, ECS, Inspector ou Flow.
- O benchmark é compilável para host e Android e participa de CI/runner físico.
- `GAP-2D-01` permanece parcial até existirem relatórios B e C e decisão final.
