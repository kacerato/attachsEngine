# OpenSSL privado para o compilador C# Android

OpenSSL **3.5.8**, fonte oficial, licença Apache-2.0 em LICENSE.txt. O runtime
.NET 8.0.27 linux-bionic vendorizado (Mono/SGen sob a ABI de hospedagem CoreCLR)
usa a ponte System.Security.Cryptography.Native.OpenSsl.
O carregamento/compilação de assemblies usa criptografia; o BoringSSL do sistema
Android não satisfaz essa ABI. A ausência causava abort ao tocar Aplicar.

Fonte: https://github.com/openssl/openssl/releases/tag/openssl-3.5.8

Arquivo: openssl-3.5.8.tar.gz, SHA256 oficial conferido:
`a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2`.

## Reprodução

`tools/build-android-openssl.sh` verifica o arquivo, configura android-arm64/API26
e constrói com o NDK. Requer Bash, GNU make, curl, tar, sha256sum e Perl completo.
Defina ANDROID_NDK_ROOT e, se necessário, MAKE/JOBS. Nesta rodada: NDK
29.0.14206865, Git Bash e mingw32-make. O Perl reduzido do Git precisou das
bibliotecas Perl da distribuição oficial Perl/perl5 v5.40.3 no PERL5LIB;
uma instalação completa de Perl compatível com o shell dispensa essa adaptação.

Exemplo em um shell com as dependências no PATH:

```sh
export ANDROID_NDK_ROOT=/c/Users/donod/AppData/Local/Android/Sdk/ndk/29.0.14206865
bash tools/build-android-openssl.sh
```

Somente a extensão das bibliotecas é personalizada: `libcrypto.so.astra.so` e
`libssl.so.astra.so`. SONAME e NEEDED concordam; ambas terminam em .so para serem
extraídas pelo Android. Gradle empacota arm64-v8a diretamente deste diretório.
Os hashes dos binários ficam em arm64-v8a/SHA256SUMS.

DotNetHost abre os caminhos absolutos antes do CLR, verifica símbolo de versão,
fixa os handles até o fim do processo e define CLR_OPENSSL_VERSION_OVERRIDE
como astra.so. Isso corresponde ao prefixo libssl.so. no
[shim oficial .NET 8](https://github.com/dotnet/runtime/blob/v8.0.27/src/native/libs/System.Security.Cryptography.Native/opensslshim.c).
Falha de dependência retorna erro antes de entrar no compilador.

Não modifica a biblioteca do Android, os algoritmos criptográficos ou o runtime
vendorizado. Não inclui engines/módulos dinâmicos nem certificação FIPS. Esta
integração valida compilação local de código; não afirma validação de TLS,
repositório de certificados ou de toda a API System.Security.Cryptography.
