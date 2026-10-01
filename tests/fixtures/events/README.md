# TimerConnection-20261001

Projeto independente produzido pelo exporter real `aether_ui_preview write-timer-connection-project <diretório vazio>`. Cena15, Timer3, SDK/nativo ABI25. Contém TimeProbe e um emissor com Timer único não escalado de 0,15 s. Seu receptor filho começa inativo e pertence a timeout-receiver.

TimerConnectionProbe exige ação/target autorais, receptor inicialmente inativo e ativação já aplicada ao receber TimerElapsed. CONNECTION PASS é a prova gerenciada esperada; TIME PASS continua verificando a cadeia de relógios. Compilar a fixture foi verificado no host; executar em Android ainda depende do fluxo real do aparelho. Parar Play deve restaurar o receptor autoral inativo. Não substitua a fixture por um script que chame SetActive por conta própria, pois mascararia ausência do consumidor de conexão.
