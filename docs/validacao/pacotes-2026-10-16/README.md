# Pacotes de terceiros: POLYGON Icons (Synty) e BoZo 2.2 (16/10/2026)

Dois pacotes da fila do usuário levados do host ao aparelho: o POLYGON Icons Pack v1.01 (520 FBX de ícones low-poly, `poly.unitypackage`) e o BoZo Stylized Modular Characters 2.2 (rig Humanoid `BodyRig.fbx` + clipes Humanoid `BMAC_*.anim`). Referências: Unity 6000.0, Model Import Settings › Rig (Avatar, Humanoid) e Animation Clip › Root Transform (Bake Into Pose, Offset).

## Host

| Verificação | Comando / cenário | Resultado |
|---|---|---|
| POLYGON: FBX aceitos pelo importador | `aether_ui_preview … import-models <Models> 12` | 12/12 aceitos (2 nós, 1 malha, 2 materiais cada) |
| BoZo: rig Humanoid + clipe idle | `aether_unity_material_tests unity_humanoid_bozo` com `AE_BOZO_PACK` | 92 nós, Avatar com 52 ossos (`pelvis`, `foot_l`, `head`); clipe 3,77 s, 53 trilhas; pés a 3 cm do chão, cabeça 58 cm acima do quadril |
| Unity clip settings | `aether_tests unity`, estúdio 73/73, Unity 24/24 | Bake Into Pose aceito; m_OrientationOffsetY aplicado ao corpo; Loop Pose, Mirror, Level e Cycle Offset continuam recusados |

O `unitypackage` foi extraído no host por um script próprio (`tar.gz` com pastas por GUID); o editor ainda não abre `.unitypackage` diretamente.

## Aparelho (Astra Dev code 95 → 96, projeto novo "Pacotes")

| Passo | Captura | Observado |
|---|---|---|
| Importar › Pasta de pacote › Astra/Pacotes/POLYGON | `aparelho/polygon-biblioteca.png` | Biblioteca · POLYGON: 6 modelos, 16 texturas |
| Modelo › detalhe | `aparelho/polygon-detalhe.png` | "2 nós · 1 malhas · 2 materiais"; aviso: o FBX cita `PolygonIcons_Texture_01.psd`, que não vem no pacote |
| Na cena (code 95) | `aparelho/polygon-na-cena-95.png` | Ícone "Add" instanciado e enquadrado, sem textura (slot só com os fatores) |
| Importar › Pasta de pacote › Astra/Pacotes/BoZo | `aparelho/bozo-biblioteca.png` | 2 modelos, 2 clipes |
| BodyRig › Perfil 3/3 › Mais de 4 influências | `aparelho/bozo-perfil-influencias.png` | "Recusar perda" → "Reduzir a 4"; Preparar |
| Resumo | `aparelho/bozo-resumo.png` | 92 nós · 1 malha · 2 materiais · 1 skin · 1 clipe; "quatro maiores influências renormalizadas por autorização da receita" |
| Na cena | `aparelho/bozo-na-cena.png` | BodyRig em pé, 2,08 m, com Animação (take do FBX) |
| Clipe BMAC_M_Idle (code 95) | `aparelho/bozo-clipe-recusado-95.png` | Recusado: "Configuração Unity sem equivalente: m_LoopBlendPositionY" |
| Clipe BMAC_M_Idle (code 96) | `aparelho/bozo-clipe-estudio-96.png` | Convertido e aberto no estúdio: 3,767 s, quaternions por osso, 92 ossos |
| Play no estúdio | `aparelho/bozo-idle-play-96.png` | Quatro quadros do idle: em pé, braços relaxados, sem salto |

Correção no aparelho (code 96, `native/platform/android/android_main.cpp`): quando o FBX cita uma fonte de trabalho que o pacote não traz (PSD/TIF do atlas), a primeira imagem exportada com o mesmo radical é usada (`Atlas_01.psd` → `Atlas_01_A.png`). Resultado do reimportar do POLYGON com essa regra: ver a última linha abaixo.

| Modelo novo do POLYGON (code 96) | `aparelho/polygon-reimportar-96.png`, `aparelho/polygon-na-cena-96.png` | "Texturas: 1 aplicadas (2 MB com mipmaps)", ASTC 6×6, 2048 px; o ícone Alarm aparece com as cores do atlas na cena |

Fora desta rodada: BoZo completo (roupas modulares, shaders estilizados, expressões por blendshape) e abrir `.unitypackage` direto no aparelho.
