// Fronteira C ABI blittable sobre o SQLite vendorizado (native/third_party/sqlite — ver
// VENDORED_COMMIT.txt para a versão exata). Implementa o binding mínimo do item 1.4.5 do plano
// ("Índice de dependências em SQLite"): abrir/fechar conexão, executar SQL sem resultado
// (DDL/INSERT/UPDATE/DELETE) e executar uma consulta parametrizada lendo linha a linha.
//
// Deliberadamente NÃO inclui o schema do índice de dependências em si (quais tabelas, quais
// colunas para "quem usa este asset") — isso é trabalho da Fase 7 (pipeline de import), quando
// existir um consumidor real que precise dessas colunas específicas. Construir esse schema agora,
// sem consumidor, seria abstração precoce (docs/PLANO-FECHAMENTO-LACUNAS.md §8: "interface sem
// dois consumidores reais"). Esta fatia entrega a fundação: um binding SQLite testado que qualquer
// schema futuro pode usar.
//
// Regra de fronteira (CONVENCOES.md §2): nada de std::string/char* confiado-em-nul cruzando esta
// API pela metade gerenciada — texto (caminho do banco, SQL, parâmetros de bind, valores de coluna
// TEXT) sempre viaja como (ponteiro UTF-8, comprimento em bytes) explícitos, nunca terminação nul
// implícita. Isso mantém os dois lados P/Invoke-friendly com ReadOnlySpan<byte>/Encoding.UTF8, sem
// depender de nenhuma conversão automática de marshalling do runtime.
#pragma once

#include "core/base.h"

extern "C" {

struct AetherSqliteConnection;
struct AetherSqliteStatement;

enum class AetherSqliteResult : ae::i32 {
  Ok = 0,
  Error = 1,          // erro genérico de sqlite3 (SQL inválido, restrição violada, etc.)
  CantOpen = 2,        // sqlite3_open falhou — caminho inacessível, permissão, disco cheio
  Busy = 3,            // banco travado por outra conexão/transação
  Misuse = 4,          // API chamada fora de ordem (ex.: Step depois de Finalize)
  Row = 5,             // Step produziu uma linha — não é erro, é o valor de sucesso "há dado"
  Done = 6,            // Step terminou sem mais linhas — sucesso, execução completa
}; // espelha o subconjunto de SQLITE_* relevante para o chamador C#; ver sqlite_bridge.cpp para o mapeamento completo

enum class AetherSqliteColumnType : ae::i32 {
  Integer = 1,
  Float = 2,
  Text = 3,
  Blob = 4,
  Null = 5,
}; // espelha SQLITE_INTEGER/FLOAT/TEXT/BLOB/NULL — ver sqlite3_column_type

// --- Conexão ----------------------------------------------------------------

// Abre (ou cria, se não existir) o banco em utf8Path[0..pathLength). Devolve nullptr em falha —
// chamador não tem como distinguir a causa exata sem outra chamada; para diagnóstico, preferir
// checar se o diretório/permissões estão corretos ANTES de chamar isto (mesma disciplina de
// StandardFileSystem.EnsureScopeExists do item 1.1.1 — este bridge não sabe nada de
// FileSystemScope, recebe sempre um caminho físico já resolvido pelo chamador gerenciado).
AetherSqliteConnection *AetherSqlite_Open(const char *utf8Path, ae::i32 pathLength);

// Fecha a conexão e libera recursos nativos. Todo AetherSqliteStatement preparado nesta conexão
// deve ser finalizado (AetherSqlite_Finalize) ANTES desta chamada — sqlite3_close falha
// (SQLITE_BUSY) se houver statement pendente, e este bridge propaga essa falha em vez de forçar
// um close destrutivo que vazaria o statement nativo.
AetherSqliteResult AetherSqlite_Close(AetherSqliteConnection *connection);

// Mensagem de erro da última operação nesta conexão, como (ponteiro UTF-8, comprimento). O
// ponteiro é válido até a próxima chamada nesta conexão — chamador deve copiar antes de invocar
// qualquer outra função do bridge. Devolve comprimento 0 se não houver erro registrado.
void AetherSqlite_GetLastError(AetherSqliteConnection *connection, const char **outUtf8Message, ae::i32 *outLength);

// --- Statement preparado (DDL/DML/query) ------------------------------------

// Prepara sql[0..sqlLength) para execução. Devolve nullptr em falha (erro de sintaxe SQL) —
// AetherSqlite_GetLastError na mesma conexão explica por quê.
AetherSqliteStatement *AetherSqlite_Prepare(AetherSqliteConnection *connection, const char *utf8Sql, ae::i32 sqlLength);

// Libera o statement preparado. Idempotente-safe do lado do chamador (chamar em um ponteiro já
// finalizado é uso indevido — UB do lado do SQLite — mas o padrão de uso esperado é sempre parear
// com exatamente um Prepare, mesma disciplina RAII do C# via IDisposable do lado gerenciado).
void AetherSqlite_Finalize(AetherSqliteStatement *statement);

// Avança o statement uma linha. AetherSqliteResult::Row indica que há dados prontos para ler via
// AetherSqlite_Column*; AetherSqliteResult::Done indica fim de resultado (statement sem SELECT, ou
// SELECT esgotado). Qualquer outro valor é erro.
AetherSqliteResult AetherSqlite_Step(AetherSqliteStatement *statement);

// Reinicia um statement preparado para nova execução (ex.: mesmo INSERT com parâmetros diferentes
// em loop) sem re-preparar o SQL — sqlite3_reset seguido de sqlite3_clear_bindings, para que o
// chamador não herde bindings de uma execução anterior por engano.
AetherSqliteResult AetherSqlite_Reset(AetherSqliteStatement *statement);

// --- Bind de parâmetros (1-based, mesma convenção do SQLite) ---------------

AetherSqliteResult AetherSqlite_BindInt64(AetherSqliteStatement *statement, ae::i32 index, ae::i64 value);
AetherSqliteResult AetherSqlite_BindDouble(AetherSqliteStatement *statement, ae::i32 index, double value);
AetherSqliteResult AetherSqlite_BindText(AetherSqliteStatement *statement, ae::i32 index, const char *utf8Value, ae::i32 length);
AetherSqliteResult AetherSqlite_BindNull(AetherSqliteStatement *statement, ae::i32 index);

// --- Leitura de coluna (0-based, mesma convenção do SQLite), após Step == Row ---

AetherSqliteColumnType AetherSqlite_ColumnType(AetherSqliteStatement *statement, ae::i32 columnIndex);
ae::i64 AetherSqlite_ColumnInt64(AetherSqliteStatement *statement, ae::i32 columnIndex);
double AetherSqlite_ColumnDouble(AetherSqliteStatement *statement, ae::i32 columnIndex);

// Ponteiro válido até a próxima chamada de Step/Reset/Finalize neste statement — mesma regra de
// GetLastError. outLength é o número de BYTES utf8 (não de caracteres), mesma convenção do
// sqlite3_column_bytes.
void AetherSqlite_ColumnText(AetherSqliteStatement *statement, ae::i32 columnIndex, const char **outUtf8Text, ae::i32 *outLength);

} // extern "C"
