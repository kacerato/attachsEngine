# Composição de animação em paralelo (24/10/2026)

Vila Astra S em Play no POCO F7, mesmo APK nos dois lados (`aether.animation_parallel`).

| APK | Lado | Thread do jogo (ms) | CPU do processo (ms) | Amostras simpleperf (20 s) |
|---|---|---|---|---|
| cortes-0256 (worker acordava a cada 1 ms) | serial | 12,72 | 13,61 | 17 564 |
| | paralelo | 10,05 | 17,92 | 32 457 |
| cortes-0328 (worker dorme até haver job) | serial | 13,11 | 14,04 | 17 308 |
| | paralelo | 12,64 | 15,33 | 23 159 |

Leitura: as fases paralelizadas (mistura pura por propriedade e TRS → Euler) eram só ~6% das
amostras do modo serial; o grosso do `SceneAnimator::advance` é a amostragem dos canais e a
montagem das contribuições (10,5% próprio + ~3,7% de keyframes + 2,9% de índices), ainda em
série. Com o worker dormindo, o ganho na thread do jogo cai para 0,47 ms e o processo gasta
1,3 ms a mais (futex/syscall 2,8%). **Decisão: desligado por padrão**; volta a ligar quando a
amostragem por personagem também for paralela. O 0256 também expôs o defeito do JobSystem
(worker ocioso a 1 ms e travamento no destrutor), corrigido no 446f03d1.
