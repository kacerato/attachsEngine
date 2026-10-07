# Canais Android — 07/10/2026

Manter duas instalações, com identidade estável. Não criar um novo applicationId
por bloco de validação: a compilação de teste utiliza a instalação Dev existente.

| Canal | Nome no aparelho | Pacote | Distribuição |
|---|---|---|---|
| Público | Astra | dev.aether.editor | APK assinado pela chave de distribuição, publicado no Download/AstraDocs |
| Desenvolvimento | Astra Dev | dev.aether.editor.u07 | Build separado, dados preservados entre blocos |

O sufixo histórico u07 é conservado para manter os dados da instalação Dev. Ele
não cria um terceiro canal. A instalação antiga dev.aether.editor.validacao foi
removida após autorização e backup verificado dos projetos. O helper de gestos
dev.aether.editor.u07.test também foi retirado; pode ser reinstalado para aceite,
mas não é um aplicativo público nem deve ganhar um launcher.

## Compilar

Público: `:app:assembleRelease` usa o pacote dev.aether.editor e nome Astra.
Configure a chave de distribuição existente por ASTRA_KEYSTORE,
ASTRA_KEYSTORE_PASSWORD, ASTRA_KEY_ALIAS e ASTRA_KEY_PASSWORD no ambiente protegido.
Não substitua essa chave pela debug; não grave senhas ou chave no repositório.
Depois: auditar APK, publicar assets/checksum/manifesto na Release pública AstraDocs,
atualizar Download/guia/nota distribuída, build/links/capturas e confirmar deploy READY.

Dev: no diretório android, executar:

```powershell
./gradlew.bat :app:assembleRelease '-PastraApplicationId=dev.aether.editor.u07' '-PastraVersionCode=8' '-PastraVersionName=0.2.2-dev.20261007'
```

O label padrão para pacote distinto do público é Astra Dev, configurável com
astraApplicationLabel. Incrementar o versionCode de cada canal quando publicar
uma revisão nova; a linha acima identifica a revisão aceita em 07/10, não uma
versão fixa para todos os builds futuros. Conservar a assinatura Dev para atualizar
esse pacote sem apagar seus projetos. Remover um app só após backup/autorização.

## Esta distribuição

A prévia pública 0.2.2-preview.20261007/código 7 é exatamente o APK já validado,
SHA 61f7d8d85581dc090ad906e865b2e96c4ff974bf14cab4b5202806c0a2075936, base 2e6ea408.
Dev foi atualizada para 0.2.2-dev.20261007/código 8 com novo label, sem alteração
da biblioteca nativa em relação ao público. Esta revisão de identificação não
reescreve a evidência histórica do U07 nem declara um novo aceite de gameplay.
