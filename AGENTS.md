# AGENTS.md — regra visual do editor Astra

Estas instruções complementam as instruções do agente já fornecidas para o projeto.

## Origem e entrega de capacidades

Toda capacidade nova de objeto, componente, node, recurso ou ferramenta de autoria deve ter uma referência concreta na documentação ou no código oficial da Unity ou da Godot, com versão e link registrados na entrega. A referência autoriza estudar a capacidade; não obriga copiar a classe, a hierarquia ou o visual da outra engine. O atlas em `docs/planos/ampliacao-2026-09-23/` é inventário de pesquisa, não catálogo de recursos já implementados.

Para expandir a quantidade de tipos, trabalhe por famílias e suas dependências reais. Cada tipo exposto no editor precisa de modelo, consumidor em runtime, propriedades com efeito, persistência, ciclo de vida, caminho de criação e cenário de validação. Não coloque no menu um nome de Unity/Godot que a Astra ainda não executa. Declare separadamente tipos implementados, parciais e pesquisados; publique a contagem real de cada pacote. Quando o pedido envolver “todos”, use o recorte versionado do atlas como universo de referência e avance por pacotes completos, sem declarar paridade antes de verificar cada capacidade.

## Interface de toda função nova ou revisada

NÃO IREI SER SIMPLISTA NO DESIGN.

Antes de considerar uma função de editor pronta, capture a tela real no dispositivo ou no ambiente executável e julgue a hierarquia, o espaço útil, a legibilidade, o fluxo de criação e edição, os estados e a coerência com a identidade Astra. Se a tela for superficial, use essa captura como referência para **gerar uma proposta visual da função** com geração de imagem. Gere também **ícones novos para os conceitos novos**, coerentes com o acervo existente; quando a interface usar atlas ou recurso vetorial, integre os ícones ao pipeline real em vez de deixar apenas a imagem conceitual.

Compare o fluxo com documentação e editor de engines maduras. Extraia princípios de descoberta, seleção e edição, adaptando-os ao toque e ao viewport mobile. Não copie sua aparência. Faça uma proposta estrutural quando o layout atual desperdiçar espaço ou exigir navegação excessiva.

A imagem gerada é uma hipótese de design, nunca evidência de implementação. Antes de codificar, confronte cada controle mostrado com modelo, serialização, runtime e ação do editor. Remova da implementação propriedades, diagnósticos e botões sem comportamento real. Implemente a mudança na UI executável e verifique o resultado em captura posterior; refine até que a função seja utilizável. Relate separadamente o que é conceito, o que foi implementado e o que foi testado no aparelho.
