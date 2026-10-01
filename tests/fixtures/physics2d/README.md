# Physics2DProbe — aceite de cena authorada

Classe Physics2DProbe, ComponentId acceptance.physics2d. Copiar Physics2DProbe.cs ao projeto e anexar UMA instância no próprio corpo dinâmico. Não cria corpos, colliders, parede ou recursos e não usa Jolt3D como fallback. Só comanda velocidade do corpo já existente no GameWorld Play; authoring não é alterado.

## Configuração pela UI real

1. Criar receita Caixa dinâmica2D, nomear PHY2D Body, posição mundial (0,0,0), rotação0, escala(1,1,1). Body2D Dynamic, mass1, gravityScale0, linearDamping0.05 ou0, fixedRotation ligado. Collider2D Box, halfX/halfY0.5, offsets0, sensor desligado, restitution0. Sem junta, força constante, tween, constraint ou PathFollow nesse corpo.
2. Criar receita Caixa estática2D, nomear exatamente PHY2D Wall, posição (4,0,0), rotação0, escala1. Body2D Static. Collider2D Box, halfX0.5/halfY2, offsets0, sensor desligado, restitution0. Mesma camada0 e ambos ativos. Nenhum outro objeto entre corpo e parede.
3. Compilar o projeto, anexar Physics2DProbe ao PHY2D Body pelo catálogo→prévia→Adicionar e entrar em Play. Reiniciar Play para repetir. Box2D não gera malha visual; gizmos/transformações são evidências separadas.

## Critérios

O probe valida fachadas tipadas reais Body2D/Collider2D e objetos ativos; consulta ray segmentoXY ignorando o corpo com identidade de parede+collider, normalunitáriaXY/fração válida; overlapcircle sobre parede sem normal inventada. Aplica velocidade(2,0)/angular0 e lê o solver imediatamente. Precisa observar pose avançar>.1u, CollisionEnter genérico vindo da parede com normalXY/Z0 e velocidadeX reduzida abaixo.25u/s, sem atravessar centro da parede. Então para a velocidade e emite PHY2D PASS. Tolerâncias não dependem de contagem exata de quadros. Prazo12s do clock real Play; pausas suspendem avanço.

Body2D não oferece flags Enabled/Simulated no schema atual, e Collider2D também não possui enabled. O script não inventa nem anuncia aceite desses controles. ActiveInHierarchy é lido, mas desativação/remoção durante Play não é testada por este cenário. Força/impulso/torque/kinematic e sensores são outros recortes; este PASS não cobre essas operações.

Logs sómarcos PHY2D WAIT/READY/ADVANCE/CONTACT/PASS/FAIL. Falta/configuração inadequada dá WAIT, nunca PASS; erro de API/invariante dá FAILúnico. READY inclui IDs e resultados das consultas. Nenhum build, teste ou ADB foi executado pelo agente ao escrever esta fixture; resultados físicos devem ser registrados após root operar a UI real.
