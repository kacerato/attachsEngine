# CMakeLists.txt escrito para este repositório — a distribuição "amalgamation" oficial do
# SQLite (ver VENDORED_COMMIT.txt) não traz build system próprio, só sqlite3.c/sqlite3.h.
#
# Configuração mínima e determinística: sem threads compartilhadas entre conexões (o Core
# gerencia sua própria concorrência — ver Aether.Core.Jobs — SQLite não precisa duplicar isso),
# sem extensões carregáveis em runtime (SQLITE_OMIT_LOAD_EXTENSION — CONVENCOES.md §2 não permite
# carregar código nativo arbitrário fora da fronteira blittable conhecida), FTS/RTREE desligados
# por padrão (não usados pelo índice de dependências do item 1.4.5 — ligar sob demanda quando
# houver consumidor real, não especulativamente).
cmake_minimum_required(VERSION 3.22)
project(sqlite3 C)

add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(sqlite3 PRIVATE
  SQLITE_THREADSAFE=2                 # serialized por conexão, não por processo inteiro — ver https://sqlite.org/threadsafe.html modo 2 (multi-thread)
  SQLITE_OMIT_LOAD_EXTENSION=1        # nenhuma extensão nativa carregável em runtime
  SQLITE_OMIT_DEPRECATED=1            # não usa nenhuma API já removida das versões suportadas
  SQLITE_DEFAULT_MEMSTATUS=0          # desliga contabilidade de memória interna do SQLite — Aether.Diagnostics.AllocationTracker (item 1.2.4) já mede alocação do lado gerenciado
  SQLITE_DQS=0                        # aspas duplas SEMPRE identificador, nunca string — evita a ambiguidade histórica que causa bugs silenciosos de SQL
)

# SQLite é C puro compilado dentro de um projeto majoritariamente C++20 com -Wall -Wextra
# -Wpedantic -Werror (ver native/CMakeLists.txt) — mas como todo outro terceiro vendorizado
# (Jolt, Box2D), este subdiretório é adicionado ANTES dessas flags serem aplicadas ao diretório
# do projeto, então elas nunca chegam a este alvo. Não há necessidade de silenciar warnings aqui.
