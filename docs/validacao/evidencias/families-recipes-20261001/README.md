# Receitas V3 — evidências

[Contrato F006 e referências](../../../planos/RECEITAS-REFERENCIAS-2026-10-01.md).

`native-initial.log`: 21/21 antes do layout responsivo. `native-final.log`: 20/21; preserva o cenário que tentava tocar um receptor fora da página visível. `native-pagination.log`: **21/21** com navegação real do seletor. São três novos cenários integrados e dezoito regressões de codec/aplicação seletiva/recursos; nenhuma suíte completa foi executada. `prefab-regression.log`: 24/24 sobre a primeira revisão das receitas; as mudanças posteriores são layout, rótulo da entrada e cenário de paginação.

`recipe-inputs-phone.png` registra o problema visual inicial. `concept-phone.png` é hipótese gerada, não evidência funcional. `recipe-inputs-final.png`, `recipe-ready-final.png` e `recipe-picker-final.png` mostram o editor executável com documento e sessão reais em 853×394. Inspeção visual: dois campos completos, rótulos distintos, destino visível, Apply indisponível antes da preparação e disponível depois, ações secundárias separadas e seletor com páginas acessíveis. O fluxo de toque/Apply/Undo é verificado pelo cenário, não inferido das imagens. Rasterização host, sem Vulkan Android. O relatório de preparação ainda conta dois clips do Inspector subjacente, coberto pela superfície temporária; campos desta superfície estão completos.

`android-build.log` e `package-manifest.json` registram build/pacote quando concluídos. Ausência de manifest ou de `BUILD SUCCESSFUL` significa que esse passo não foi encerrado. ABI continua 34; versão do arquivo de receitas é independente. Sem novos tipos, fachadas ou ícones: utiliza a composição e consumidores existentes.

Não houve ADB, instalação ou execução física desta revisão. Capturas host e igualdade de assets no APK não são prova de CLR/Vulkan/toque no aparelho.
