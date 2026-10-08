# B1 — controllers compartilhados e overrides: aceite 08/10/2026

Repositório confirmado: `https://github.com/kacerato/attachsEngine.git`, branch de trabalho `codex/gameplay-runtime`, base `23de57c3`. B1 fecha recurso de controller, execução independente, overrides de clipes e workflow integrado. Não encerra U03/U06/U08 nem todo o roadmap universal.

## Gates e artefatos

- Host: **11/11** cenários focados, `host-tests.log`. Poses reais, parâmetros independentes, v1/v2/v3, recurso inválido/ausente, revisão, prefab/máscaras, publicação/histórico e reabertura. Não executada a suíte inteira.
- SDK `Astra.Scripting` Release: zero erros/avisos; fachada regenerada com `GetController/SetController` e máscaras locais. A publicação Android inclui avisos preexistentes CS8981 de `math/quaternion`, sem erro.
- Android nativo e Gradle Release: sucesso. A correção final foi somente a legenda de autoridade; recompilada, empacotada, reinstalada e verificada novamente.
- APK final: **0.2.6-dev.20261008**, code **14**, `dev.aether.editor.u07`, **103836602 bytes**, SHA-256 **6a940fdcf389c2d3c8cf386e33abb2e7f7f67f8fc72e28799b0464a1b77db7d8**. Caminho local `build/astra-dev-animator-controllers-20261008.apk`. Hash do `base.apk` instalado igual ao local.
- POCO F7 / Android 16: atualizado com `install -r`; mantidas apenas as instalações público `dev.aether.editor` e Dev `dev.aether.editor.u07`. Sem desinstalação nem remoção de projetos pessoais.

## Cenário real, generalidade e autoria

Projeto `AnimatorControllers-20261008`, escrito pelo serializador/importador real e copiado para a área externa Dev. Dois mecanismos glTF sem skin, cada um com filho Painel, compartilham `Mecanismos.aeanimator`. B troca Meia abertura por Aberta. A primeira versão de fixture gerada com ferramenta antiga foi descartada do aceite; a ferramenta recompilada gerou o projeto usado, com parâmetro manual Abertura.

No aparelho: abrir A → Recurso → Copiar recurso → Undo da atribuição mantém o arquivo disponível → Editar recurso original → adicionar estado → Undo/Redo → arrastar estado → Undo/Redo → abrir B. B mostra a nova revisão **7**, o estado compartilhado e seu override local. Reset, seleção de clipe substituto, Undo/Redo e escolha do mesmo controller conferidos. Gaveta rolada até Corpo/Máscara, sem perder nome/revisão. Salvar → force-stop → reabrir: GUID compartilhado, revisão 7, estado e par de B preservados. A cópia não atribuída continua registrada.

Arquivo retornado do aparelho confirma GUID `cba0dfd4e553c6e3591a84a85350def5`, revisão 7 e próximo ID 39; B mantém o par `ab187bf1fcd384e83826b3bc57855a78 → 74184828f526b1e90efa1529a606a725`. A tem zero pares. Arquivos não foram corrigidos à mão para produzir o resultado. O estado de autoria não é substituído pelos valores da sonda em Play.

Capturas `before-device.png`, `resource-device.png`, `shared-edit-device.png`, `override-device.png`, `local-references-device.png`, `cold-reopen-device.png` e **`final-device.png`** mostram a UI executável e o atlas real. O grafo domina a superfície; uma gaveta contextual atende recurso/instância. A última captura corrige a legenda que antes dizia Play quando a topologia estava somente protegida na instância. Não houve geração conceitual apresentada como prova; dois conceitos novos entraram como vetores e atlas reais.

## Runtime no aparelho

Sonda C# `tests/fixtures/animator/SharedAnimatorProbe.cs`, compilada pelo próprio projeto no Android, usa APIs públicas. **Seis checks PASS**, repetidos depois da reinstalação final, em `final-runtime.txt`:

1. Controller válido e igual nos dois mecanismos sem skin.
2. Mesmo parâmetro, poses reais diferentes por override.
3. Valores independentes A=0/B=1.
4. GUID desconhecido recusado com `UnknownResource`.
5. Desvincular incorpora grafo/clipes: pose preservada e controller local.
6. Reatribuição após desvincular limpa overrides: poses correspondentes.

Nenhum `FAIL` ou erro C# nesses checks. Recurso, canais e consumidor são reais; não há código especial no runtime para estes nomes de objeto.

## Revisão de todos os quadros

`device-motion.mp4`, SHA e PTS em `frames/extraction.json`/`frames.csv`: **537 quadros**, 0 pulados, duração até PTS **8.911 s**, 1920×886. Todas as **18 folhas** foram examinadas, cada quadro indexado; conferidos adicionalmente frames completos 56, 296 e 536. A região do mecanismo não cobre a tela toda e corta parte de B depois da reatribuição; frames completos suplementam esse limite.

Frames 0–24 mostram autoria; 25–55 mostram início do Play com painéis fechados. 56–115 mostram poses distintas após o comando manual. 116–295 mostram A fechado/B aberto. 296–536 mostram reatribuição e poses correspondentes; diferença de perspectiva/posição continua normal. Não houve sumiço ou corrupção visual. A sonda usa clipes de poses e parâmetros em **degraus**, portanto as mudanças discretas são deliberadas: esta gravação verifica override/ownership, não crossfade contínuo, caminhada humana ou foot sliding. Aviso DTS na saída PNG não descartou quadro: contagens de ffprobe e extração são iguais. A gravação precede somente a correção de legenda; os seis checks foram repetidos no APK final.

## Limites e migração

V1/v2 legíveis como grafos locais; v3 acrescenta controller/pares. APK anterior não lê v3: backup antes de salvar. Recurso `AEANIMATOR 1`. Play utiliza snapshot em memória; arquivos são recarregados na autoria, sem I/O por quadro. Ausência não ativa grafo antigo como fallback. Overrides órfãos ficam persistidos e diagnosticados.

Ainda faltam editor visual autônomo de recurso não atribuído, autoria aninhada por SDK, submáquinas, interrupções, aditivas, root motion, retargeting, IK, contato de pés, edição de clipes/Timeline, tracks de outras propriedades e escala de muitas instâncias. A edição visual desta entrega exige atribuir o recurso a um Animator; recurso não atribuído abre no IDE textual. Não há alegação de suporte ilimitado ou qualidade comprovada para todo personagem.

Referências e arquitetura: `docs/planos/ui-universal/ANIMATOR-CONTROLLERS-2026-10-08.md`. Guia e nota em AstraDocs, disponibilidade **development**; distribuição pública 0.2.3 permanece intacta. Publicação e SHAs são confirmados na entrega após o deploy.

## Espaço no host

Arquivos antigos de build foram compactados com NTFS mantendo bytes, projetos e assets. Uma rodada de 140 objetos caiu de 994034314 para 329424896 bytes; outros executáveis/bibliotecas também foram compactados. As tentativas de exclusão foram rejeitadas pela revisão automática com único motivo `blocked by policy`, apesar da autorização; não houve contorno da recusa nem alegação de arquivos apagados.
