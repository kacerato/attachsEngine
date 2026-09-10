# Retirada do armazenamento global de água da entidade

Data: 09/09/2026. Plano externo, seção 12.

## Alteração efetiva

EditorEntity não contém mais water[34], waterLayout[9] nem as três flags de
ativação global. EditorDocument não copia esses campos individualmente.
Dados históricos passam a ocupar um valor opcional na coleção genérica de
componentes; entidades secas não alocam esse valor, inclusive após salvar/reabrir.

LegacyWaterSettings possui campos nomeados, como density, windSpeed, seed,
minimumWavelength e budgetMiB. Não contém array de armazenamento dos valores.
legacyField é a tradução explícita dos índices históricos para membros tipados;
é usada pelo codec/adaptador de propriedades e pela ponte de configuração legada.
O consumidor Android usa nomes para as propriedades individuais. Este contrato
é compatibilidade temporária, não a reintegração de água opcional futura.

## Migração e identidade

A versão v8 continua sendo escrita com sua seção escalar histórica. A coleção
omite esse payload no registro de componentes para não criar duas fontes de
verdade no arquivo. A leitura migra os escalares para armazenamento opcional e
remove valores totalmente padrão/inativos antes de publicar a cena. Componentes
de rota/corpo e os demais registros permanecem com suas representações anteriores.
Ler um valor padrão não aloca. Cópia/histórico usam clone, sem alias mutável.

Não foi apagado arquivo de projeto, cena ou asset do usuário. Formatos anteriores
continuam passando pelos leitores existentes com limites históricos congelados.

## Validação

762/762 testes host: migrações anteriores, física, UI, histórico, armazenamento
seco sem payload, campos nomeados, roundtrip v8 exato e isolamento das cópias.
Debug/Release e testes Java passaram. Logs refundacao-water-storage-* em build.

## Limite explícito

O enum legado Water ainda existe. editor_properties.h, arquivo e ponte Android
ainda incluem o adaptador de água, e o build ainda contém renderer/simulação de
água. Portanto a prova de compilar e distribuir sem pacote de água não está
concluída. A próxima retirada deve separar o codec histórico e os consumidores
de produção; não apresentar este payload de migração como novo componente de
autoria nem reintroduzir painéis especiais no fluxo seco.
