# Contratos de componentes compartilhados

Data: 09/09/2026. Relacionado ao plano externo, seções 4, 8 e 12.

## Problema comprovado

Os valores de corpo físico, personagem e olhar incluíam editor_document.h.
Consequentemente um consumidor de dados, inclusive um futuro loader de jogo,
precisava importar a entidade de autoria e seus campos legados de água.

## Decisão

ComponentValue, ComponentType, ComponentNumber e Components pertencem agora a
native/scene/components.h. PhysicsBody, Character e CameraLook são contratos de
dados nessa mesma camada. Seus includes alcançam somente core/base.h e a STL.
Não possuem UI, mundo físico, documento, renderer ou recurso GPU.

O editor usa aliases nos headers existentes e conserva apenas os adaptadores
que localizam/anexam dados a EditorEntity e aplicam ações no documento. Não há
duas definições dos mesmos valores. Os consumidores atuais passam a utilizar
os contratos compartilhados sem alteração de TypeId, versão ou payload salvo.
Ícones, nomes visuais e composição do Inspector permanecem no catálogo do editor.

## Alternativas

- Duplicar structs para runtime e editor criaria duas validações e migrações.
- Mover o documento inteiro agora carregaria dados legados e estado de autoria
  para o runtime, preservando o problema sob outro diretório.
- Remover os headers antigos imediatamente quebraria consumidores sem necessidade.

## Ownership e execução

A coleção possui unique_ptr por valor. Cópias e substituições clonam os dados;
não existe alias mutável entre autoria, histórico e execução. Descritores têm
vida estática; identidade por ponteiro é apenas o token local de tipo, enquanto
TypeId textual e versão são o contrato persistente. Acesso é feito na thread
proprietária; não foi introduzida concorrência. Leitura valida em coleção
temporária e publica somente após sucesso; tipo desconhecido mantém o destino.

## Evidência e limite

test_scene_components.cpp compila incluindo apenas os contratos scene e o
harness. Carrega os três tipos, verifica valores, isolamento de mutações e
rejeição transacional de tipo desconhecido. A suíte completa preserva os testes
de versões anteriores, Inspector, histórico e simulação.

Isso não constitui um runtime de jogo independente: EditorPlayScene ainda é
um adaptador que possui EditorDocument, e a entidade genérica ainda contém água.
A extração desses contratos reduz uma dependência real e permite continuar
a separação; não fecha os gates M2, M7, M10 ou a prova sem água da seção 12.
