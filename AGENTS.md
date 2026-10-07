# AGENTS.md — regra visual do editor Astra

Estas instruções complementam as instruções do agente já fornecidas para o projeto.

## Documentação pública — Astra Docs na Vercel

Por solicitação do proprietário em 06/10/2026, o destino de pedidos futuros para
“atualizar docs” é o repositório https://github.com/kacerato/AstraDocs,
checkout local `C:/Users/donod/Downloads/AstraDocs`, domínio `astraengine.com.br`,
projeto Vercel `astra-docs`, equipe `lucas-df5f8b19`. O antigo `portal/` é a cópia
da primeira entrega; novas edições devem ser feitas no AstraDocs para evitar divergência.
Atualizar apenas documentos internos em `docs/` não encerra esses pedidos.
Siga o README do AstraDocs: atualize o conteúdo e os contratos afetados, regenere HTML,
Markdown e JSON juntos, confira o resultado, faça commit/push e confirme o deploy.
Nunca publique fontes privadas, evidências internas, credenciais ou o repositório
inteiro. Registre o estado real do deploy e mantenha explícitas as lacunas editoriais
e de validação da engine. Novos snapshots precisam de rotas próprias; não misture
Astra atual/C# com planos de Astra 2/Luau.

## Registro obrigatório de atualizações

Toda novidade, correção, mudança de comportamento, conteúdo e migração do app ou
das docs deve passar pela aba Atualizações **do site**, sem criar uma aba no APK.
Registre uma nota nova no `AstraDocs/data/updates.json`, incluindo origem App/Docs,
versão, disponibilidade, uso, validação e migração. Não reescreva notas publicadas
nem trate uma revisão local como APK distribuído. Publique a nota conforme o
README do AstraDocs; o build do portal exige cobertura do conteúdo. Assets e
distribuições também devem aparecer na revisão da entrega.

## Origem e entrega de capacidades

Toda capacidade nova de objeto, componente, node, recurso ou ferramenta de autoria deve ter uma referência concreta na documentação ou no código oficial da Unity ou da Godot, com versão e link registrados na entrega. A referência autoriza estudar a capacidade; não obriga copiar a classe, a hierarquia ou o visual da outra engine. O atlas em `docs/planos/ampliacao-2026-09-23/` é inventário de pesquisa, não catálogo de recursos já implementados.

Para expandir a quantidade de tipos, trabalhe por famílias e suas dependências reais. Cada tipo exposto no editor precisa de modelo, consumidor em runtime, propriedades com efeito, persistência, ciclo de vida, caminho de criação e cenário de validação. Não coloque no menu um nome de Unity/Godot que a Astra ainda não executa. Declare separadamente tipos implementados, parciais e pesquisados; publique a contagem real de cada pacote. Quando o pedido envolver “todos”, use o recorte versionado do atlas como universo de referência e avance por pacotes completos, sem declarar paridade antes de verificar cada capacidade.

## Interface de toda função nova ou revisada

NÃO IREI SER SIMPLISTA NO DESIGN.

Antes de considerar uma função de editor pronta, capture a tela real no dispositivo ou no ambiente executável e julgue a hierarquia, o espaço útil, a legibilidade, o fluxo de criação e edição, os estados e a coerência com a identidade Astra. Se a tela for superficial, use essa captura como referência para **gerar uma proposta visual da função** com geração de imagem. Gere também **ícones novos para os conceitos novos**, coerentes com o acervo existente; quando a interface usar atlas ou recurso vetorial, integre os ícones ao pipeline real em vez de deixar apenas a imagem conceitual.

Compare o fluxo com documentação e editor de engines maduras. Extraia princípios de descoberta, seleção e edição, adaptando-os ao toque e ao viewport mobile. Não copie sua aparência. Faça uma proposta estrutural quando o layout atual desperdiçar espaço ou exigir navegação excessiva.

A imagem gerada é uma hipótese de design, nunca evidência de implementação. Antes de codificar, confronte cada controle mostrado com modelo, serialização, runtime e ação do editor. Remova da implementação propriedades, diagnósticos e botões sem comportamento real. Implemente a mudança na UI executável e verifique o resultado em captura posterior; refine até que a função seja utilizável. Relate separadamente o que é conceito, o que foi implementado e o que foi testado no aparelho.
