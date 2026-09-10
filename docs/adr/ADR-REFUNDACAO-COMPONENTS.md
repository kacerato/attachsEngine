# Componentes autorais registrados — primeira integração

Data: 09/09/2026. Escopo: dependência de M2 e §12 de PROMPT_REFUNDACAO_ASTRA.md.

## Problema e decisão

O armazenamento opcional de traçado ainda era um membro específico da entidade,
e seu conteúdo permanecia embutido na lista de propriedades numéricas do arquivo.
Agora EditorEntity contém EditorComponents. A coleção não conhece água, renderer
ou os tipos registrados. Cada valor implementa clone, validação, leitura/escrita
e fornece um descritor estático com ID e versão. Não há RTTI nem registro global
mutável. O leitor recebe explicitamente um registro; o padrão atual inclui o
adaptador legado astra.water.route, versão 1.

Uma coleção vazia não aloca payload. Limite: 64 tipos por entidade, uma instância
por tipo nesta versão. Esse limite não define a futura política de múltiplas
instâncias. IDs conflitantes são rejeitados. Descritores e factories são código
confiável da engine e devem ter lifetime estático; o ID persistente não é um
ponteiro. Novos tipos exigem implementação e registro explícitos, não basta uma
string no arquivo.

Cópias clonam profundamente cada payload, inclusive com referências mutáveis
anteriores à cópia. Movimentos transferem ownership. Atribuição prepara a cópia
antes da troca. Isso preserva snapshots do histórico; copy-on-write não protegeria
referências previamente entregues. A coleção não cria concorrência: edição na
thread dona da sessão. Copiar componentes presentes aloca; ainda não há benchmark
para cenas grandes. A alternativa anterior EditorOptionalValue foi removida após
migração de todos os consumidores, evitando dois contratos concorrentes.

## Arquivo v8 e compatibilidade

O cabeçalho passa a AETHER_EDITOR 8. A base mantém os IDs e defaults legados
0–78. Depois dos flags vêm N registros com ID textual, versão e payload delimitado
por aspas. O codec do tipo lê somente seu payload; sobras são erro. O traçado
preserva os 16 pontos, inclusive valores inativos, para não perder autoria.

Cenas v1–v7 continuam legíveis. Os antigos campos de rota são convertidos para o
componente; defaults sem rota não criam componente. Uma migração não pode
reintroduzir um tipo ausente do registro fornecido. Tipo desconhecido, versão não
suportada, duplicata, codec inválido ou conteúdo truncado rejeitam a carga inteira.
A cena anterior permanece. saveEditorDocument valida a recarga com o mesmo
registro antes da substituição atômica; componentes ausentes do registro impedem
a gravação, em vez de serem descartados. O limite global de arquivo segue 32 MiB.

APKs anteriores não abrem v8. Rollback exige backup v7; não trocar só o cabeçalho.

## Integração e limites

O adaptador editor_route_component.h é consumido por propriedades/Inspector,
edição de pontos e extração de runtime. editor_document.h não inclui mais
water_route.h e não possui membro route. A coleção e o histórico tratam todos os
tipos do mesmo modo; os IDs antigos do Inspector encaminham ao adaptador.

Isso não conclui §12: os demais arrays/flags de água ainda estão na entidade,
o catálogo de propriedades não é um registro de Inspector extensível, e a
composição padrão ainda registra água. Não há build completo sem pacote de água,
API de scripts/NoCode ou ciclo de vida runtime de componentes implementados por
esta mudança. O próximo trabalho é migrar os demais dados e metadados para tipos
registrados, depois retirar os adaptadores legados respeitando os gates do plano.

## Provas

740/740 testes nativos: tipos independentes de água compartilham comandos e
arquivo; snapshots preservados; undo/redo; duplicação/exclusão de rota; migração
v7; IDs desconhecidos/duplicados, versões incompatíveis e payload inválido
rejeitados sem trocar estado. Testes existentes de seleção/edição/projeção passaram.
Logs build/refundacao-components-host-build.log e -host-tests.log.

Android: 15/15 testes Java, Debug/Release (33s) e salvar/reabrir v8 no aparelho
passaram. Backup v7 e evidências constam em REFUNDACAO-ASTRA.md. A prova Android
foi de projeto seco; rios foram cobertos pelos testes nativos. Build incremental.

## Extensão do contrato: volume e metadados (09/09/2026)

astra.water.body v1 agora usa EditorComponentType.numbers; seu codec, validação
e Inspector legado compartilham nomes, limites e acesso tipado. Os campos de
volume/flags foram retirados de EditorEntity. O traçado e o volume permanecem
tipos independentes. Compatibilidade v8 mantém slots legados somente como entrada
de migração e defaults na escrita; EditorComponents.merge rejeita duplicatas em
vez de escolher uma representação. O envelope não precisou mudar de versão.
A prova e os limites atualizados estão na continuação de REFUNDACAO-ASTRA.md.
