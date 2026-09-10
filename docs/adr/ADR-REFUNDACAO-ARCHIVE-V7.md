# Arquivo autoral v7: valores numéricos esparsos

Data: 2026-09-09. Preparação da migração autoral; não encerra M1/M2 nem a
extração de água exigida pelo §12 do plano.

## Problema e decisão

V6 repete todas as propriedades numéricas de todas as entidades, incluindo
dezenas de pontos de rio vazios. V7 grava somente diferenças em relação aos
padrões, sem descartar configuração desativada. Reutiliza os IDs estáveis do
contrato atual de propriedades; não cria outra lista de arrays de água.

Alternativas: manter v6 repetitivo; excluir campos apenas por enabled (perderia
configurações); introduzir imediatamente um formato de componentes sem seu
registro/consumidores (criaria um contrato fictício). A decisão é um adaptador
de migração limitado, preservando o documento existente até sua substituição.

## Gramática e invariantes

Cabeçalho: `AETHER_EDITOR 7 fingerprint entityCount`.
Cada entidade mantém identidade, pai, tipo, nome entre aspas, transform e flags
básicos como v6. Depois de material.enabled, grava `N` seguido por N pares
`propertyId value`. Os IDs são os índices estáveis atuais a partir de 9;
Transform continua na parte fixa. Ao final permanecem os flags de água,
contagem da rota, física e modo infinito existentes em v6.

Ausência significa padrão **da versão 7**, não propriedade desconhecida.
Os padrões e IDs atuais ficam congelados por contrato: mudança exige nova
versão/migração. Um fixture v6 literal testa equivalência de todos os padrões;
reordenar/adicionar campos ou trocar padrões não deve passar silenciosamente.
Esse contrato não usa nomes traduzidos nem offsets de memória no arquivo.

IDs duplicados, fora do registro, contagem excessiva, valores não finitos ou
fora dos limites são rejeitados. Leitura ocorre em documento temporário e só
publica após validar tudo. Os setters podem ativar flags; os flags persistidos
são restaurados depois, preservando configurações desativadas.

## Compatibilidade e gravação

Leitura de v1–v6 permanece; toda nova gravação usa v7. O shell Android aceita
o cabeçalho v7 para escolher a fonte; a validação integral permanece nativa.
Versões futuras são rejeitadas. Save mantém validação e substituição atômica.

APKs antigos que só leem até v6 não abrem v7. Rollback requer a cópia v6 anterior
à gravação; não trocar apenas o cabeçalho. Na prova de dispositivo, o arquivo v6
foi copiado para build/refundacao-archive-device-before.aescene antes de salvar.

## Limitações

EditorEntity ainda contém dados específicos e flags fixos de água. V7 reduz
o arquivo padrão e torna os IDs explícitos, mas não é um registro extensível de
componentes, não reduz o tamanho em memória da entidade e não elimina headers
de água. Essas mudanças exigem o registro genérico e migração próprios. Não
apresentar a versão do arquivo como conclusão da refundação.
