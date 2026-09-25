# AGENTS.md — regra visual do editor Astra

Estas instruções complementam as instruções do agente já fornecidas para o projeto.

## Interface de toda função nova ou revisada

NÃO IREI SER SIMPLISTA NO DESIGN.

Antes de considerar uma função de editor pronta, capture a tela real no dispositivo ou no ambiente executável e julgue a hierarquia, o espaço útil, a legibilidade, o fluxo de criação e edição, os estados e a coerência com a identidade Astra. Se a tela for superficial, use essa captura como referência para **gerar uma proposta visual da função** com geração de imagem. Gere também **ícones novos para os conceitos novos**, coerentes com o acervo existente; quando a interface usar atlas ou recurso vetorial, integre os ícones ao pipeline real em vez de deixar apenas a imagem conceitual.

Compare o fluxo com documentação e editor de engines maduras. Extraia princípios de descoberta, seleção e edição, adaptando-os ao toque e ao viewport mobile. Não copie sua aparência. Faça uma proposta estrutural quando o layout atual desperdiçar espaço ou exigir navegação excessiva.

A imagem gerada é uma hipótese de design, nunca evidência de implementação. Antes de codificar, confronte cada controle mostrado com modelo, serialização, runtime e ação do editor. Remova da implementação propriedades, diagnósticos e botões sem comportamento real. Implemente a mudança na UI executável e verifique o resultado em captura posterior; refine até que a função seja utilizável. Relate separadamente o que é conceito, o que foi implementado e o que foi testado no aparelho.
