# M07/M08 — submeshes e materiais autorais (Entrega 2)

13/09/2026 · branch `codex/gameplay-runtime`, sobre o commit `88a1ca6a` (Entrega 1). Segunda entrega do [pacote delegado](DELEGACAO-M08-M09-IMPORTACAO-AUTORAL.md). **M07, M08 e M09 continuam abertos**: texturas/PBR (Entrega 3) e dependências externas/codecs (Entrega 4) não começaram. Nenhuma expansão além do plano foi incluída.

## O que foi implementado

| Contrato do plano | Implementação | Onde |
|---|---|---|
| Nó com várias primitivas continua sendo um nó autoral | `MeshRenderer` v3: o slot 0 fica nos campos de sempre e os slots 1..n em `submeshes` (identidade da primitiva, slot resolvido, material do projeto e substituição local por slot). Instanciar não cria mais filhos por primitiva | `native/scene/mesh_renderer.h`, `editor_session.cpp` |
| Extração e picking com o mesmo ObjectId e o slot | Um desenho por slot, todos com o mesmo `objectId`; limites do objeto são a união dos slots; um candidato de toque por slot que seleciona o mesmo objeto | `editor_map_scene.*`, `buildPickCandidates` |
| Recurso de material com identidade, revisão, referências e serialização | `MaterialAsset` (`ASTRA_MATERIAL 1`) em `Materiais/<nome>.material`, registrado como `Material` no registro com hash do conteúdo. Carregado com o registro; arquivo ilegível vira aviso no console, e o slot volta ao material da fonte | `native/resources/material_asset.*` |
| Inspetor: slots com nomes, material atribuído e alcance | Aba Material por slot: "Slot k/n · nome · da fonte / do projeto / ausente", alcance **Esta instância** ou **Compartilhado**, remover substituição local, criar material do projeto a partir do slot, lista de escolha (fonte, materiais do projeto, novo) e os 11 campos no alcance escolhido | `editor_screen.cpp`, `editor_session.cpp` |
| Trocar material sem duplicar malha nem reconstruir geometria | Material resolvido na extração (substituição local > material do projeto > fonte); nenhuma operação de material republica geometria (verificado por contador). Edição compartilhada sinaliza o shell para republicar só os desenhos | `EditorMapScene::slotMaterial`, `takeAppearanceChanged` |
| Nomes dos materiais da fonte | O importador guarda `materialNames`; o slot mostra o nome do material do arquivo | `gltf_import.*` |
| Migração dos filhos artificiais | Na abertura e na reimportação, partes `primitive` de um nó viram slots **só** quando todas estão presentes, sem filhos, sem outros componentes, sem referência, com pose e nome da base e o objeto do nó sem malha. Material local da parte é preservado no slot. Qualquer dúvida mantém as partes, com diagnóstico no console | `editor_import_reconcile.cpp` |
| Reimportação com contagem de primitivas diferente | O vetor de slots é um campo base/novo/local: sem edição local, segue a fonte; mudança que descartaria material local de um slot vira conflito e o slot fica | idem |

### Formatos e compatibilidade

- `astra.render.mesh` v3 lê v1 e v2; `astra.import.link` v2 lê v1 (vínculos já gravados no aparelho pela Entrega 1).
- `ASTRA_MATERIAL 1` é novo. Cena `AETHER_EDITOR 12` e registro `AETHER_ASSETS 1` inalterados.
- APK anterior abrindo cena nova: o componente de malha v3 não é reconhecido por versão; a leitura falha em vez de adivinhar slots. Não abrir projetos novos com APK antigo.

## Limitações declaradas

- Material do projeto cobre fatores escalares; texturas, modo de alfa e dupla face são da Entrega 3.
- Editar material compartilhado grava o arquivo e não entra no Desfazer da cena; a substituição local entra.
- Material da fonte não é editável como compartilhado: é preciso criar o material do projeto (a fonte GLB nunca é regravada).
- Ajustar colisor à malha e água usam o slot 0.
- Excluir um arquivo de material ainda republica a biblioteca de geometria junto (caminho comum de exclusão de recursos).
- Partes legadas com dados locais ficam como partes até o usuário resolver; não há comando de consolidação manual.

## Evidências

Host (`build/editor-host`, depuração), [testes da Entrega 2](../validacao/evidencias/m08e2-20260913/host-m08e2.log):

- nó com três primitivas vira um objeto com três slots, três desenhos e três candidatos de toque do mesmo objeto, ida e volta do arquivo;
- registros de malha v2 e vínculos v1 continuam carregando;
- partes legadas viram slots preservando o material local; com script numa parte, nada é consolidado;
- reimportação 3 → 2 primitivas segue a fonte numa instância e relata conflito na que tinha material no slot removido;
- **conclusão observável do plano:** trocar só um slot numa instância (A · slot 2, desfazer/refazer), criar material do projeto a partir de A · slot 1, usá-lo em B · slot 1, editar como compartilhado a partir de B e ver A e B mudarem, irmãos intactos, nenhuma republicação de geometria, e tudo de volta depois de salvar e reabrir.

[Suíte](../validacao/evidencias/m08e2-20260913/host-suite.log): **877/879**, com as mesmas duas falhas antigas de console/barra do IDE. Dois testes anteriores foram atualizados ao contrato novo sem perder verificação: o de arquivo v1 do componente de malha (agora corta também a cauda v3) e o de controles numéricos de material (o campo de rugosidade passou a ser o campo por slot, no alcance da instância, pelo mesmo histórico).

[Arquivos reais](../validacao/evidencias/m08e2-20260913/real-reimport.log), `aether_tests --reimport-glb`: Torre, Ford, CarConcept e Porsche continuam com republicação sem mudança e reabertura idêntica. O CarConcept passou de 250 para **202** objetos vinculados em duas instâncias (101 nós × 2): os filhos por primitiva deixaram de existir.

Aparelho (APK `18A8EC23634FEBB481583387B025E578B063A0C8208F171E799EDF5406A401EC`, instalado sem apagar dados): seção seguinte.

**Incidente na conferência, registrado e revertido.** Na primeira abertura com este APK, `Conjunto` e as duas instâncias `Veiculo` não apareciam. A comparação da cena puxada do aparelho com a cópia anterior mostrou que só a flag `visible` de 11 entidades tinha mudado (pai e pose intactos) e que o arquivo fora gravado pelo APK **anterior** (componente de malha ainda v2): os arrastos feitos na hierarquia para rolar a lista, na conferência da Entrega 1, esconderam esses objetos. Não é efeito da Entrega 2. A cena do projeto de conferência foi restaurada com a cópia anterior (`EDD4491D…0FC0`), com o app parado. Pendência de UX anotada: um arraste vertical na hierarquia não deveria alternar visibilidade.

Conferência (capturas só com o editor em primeiro plano, projeto `M08Recursos0913k`):

1. **Cena anterior abre com o formato novo.** Depois da restauração, a cena gravada com componente de malha v2 e vínculo v1 abriu com Ford, Porsche, painéis e as instâncias da Entrega 1.
2. **Fonte com três materiais.** `m08e2-painel.glb` (gerada por `--write-m082-fixtures`) em Arquivos → Reimportar: prévia "1 nó · 3 malhas · 4 materiais" (três do arquivo e o neutro). [Captura](../validacao/evidencias/m08e2-20260913/previa-tres-materiais-adb.png).
3. **Um objeto só.** *Importar na cena* criou `Painel` sem filhos, com "1 componentes" (o vínculo não conta mais) e a linha de vínculo "igual à fonte". [Captura](../validacao/evidencias/m08e2-20260913/painel-um-objeto-adb.png).
4. **Aba Material por slot.** "Slot 1/3 · Pintura · Material da fonte", alcance *Esta instância / Compartilhado*, ação de criar material do projeto e campos com os valores da fonte. [Captura](../validacao/evidencias/m08e2-20260913/aba-material-slots-adb.png).
5. **Alcance da instância.** Cor G = 0.8 digitada pelo teclado numérico do sistema: o slot 1 ficou amarelo só neste objeto e a linha passou a "substituição local", com *Remover substituição local*. [Captura](../validacao/evidencias/m08e2-20260913/substituicao-instancia-adb.png).
6. **Material do projeto.** No slot 2 (Vidro), *Criar material do projeto a partir deste slot* gravou `Materiais/Vidro.material` (revisão 1, azul da fonte) e passou o alcance a *Compartilhado*. [Arquivo](../validacao/evidencias/m08e2-20260913/material-criado-adb.log), [captura](../validacao/evidencias/m08e2-20260913/material-projeto-criado-adb.png).
7. **Instância B independente.** *Instanciar* criou outro `Painel`; o slot 1 dele mostrou "Pintura · Material da fonte" com G = 0: a substituição local de A não vazou. [Captura](../validacao/evidencias/m08e2-20260913/instancia-b-sem-substituicao-adb.png).
8. **Lista de escolha e alcance compartilhado a partir de B.** No slot 2 de B, a lista ofereceu *Material da fonte*, *Vidro · do projeto · compartilhado* e *Novo material do projeto*; escolhido Vidro e o alcance *Compartilhado*, Cor R = 1 foi gravado no recurso ("Material compartilhado atualizado em todos os usos"). `Vidro.material` passou à revisão 2 (0,0,1 → 1,0,1) e a cena salva referencia o mesmo material nos slots 2 de A e B. [Lista](../validacao/evidencias/m08e2-20260913/seletor-material-adb.png), [B usando Vidro](../validacao/evidencias/m08e2-20260913/instancia-b-usa-vidro-adb.png), [edição](../validacao/evidencias/m08e2-20260913/edicao-compartilhada-adb.png), [arquivo e cena](../validacao/evidencias/m08e2-20260913/edicao-compartilhada-adb.log).
9. **Play/Stop.** Em execução, o painel aparece com o slot amarelo da substituição local; ao parar, a cena salva continuou byte a byte igual (`3B7B35C4…`) e o material na revisão 2. [Captura](../validacao/evidencias/m08e2-20260913/play-materiais-adb.png).
10. **Reabertura.** APK `0A60F155A7A935E85B57614693248A959EFD4665CCD2890C69761610158D654C` (com as correções abaixo) instalado por cima, force-stop e projeto reaberto: cena idêntica (`3B7B35C4…`), as duas referências a Vidro, material na revisão 2 e a fonte do painel reidratada. [Log](../validacao/evidencias/m08e2-20260913/reabertura-adb.log), [captura](../validacao/evidencias/m08e2-20260913/reaberto-adb.png).

Limite desta conferência: as instâncias A e B nasceram na mesma posição e o slot 2 usa a mesma geometria do slot 0, então a mudança de cor compartilhada é comprovada pelos arquivos (material e cena) e pelo inspetor, não por uma comparação visual com câmera e iluminação controladas.

## Pendências desta entrega

- Desfazer para edição de material compartilhado (hoje só a substituição local entra no histórico).
- Comando explícito para resolver partes legadas que não puderam virar slots.
- Colisor e água usando todos os slots.
- Comparação visual controlada dos alcances (câmera, iluminação e instâncias separadas); ela ganha sentido completo com texturas, na Entrega 3.
- Aceitação AST/GFX com texturas e modos de alfa depende da Entrega 3.

Defeitos vistos no aparelho e corrigidos: rótulos da lista de escolha encostados na borda (primeira letra cortada) e cabeçalho da seção de material alto demais para inspetores baixos (os campos saíam da tela; também quebrava o teste de controles numéricos).
