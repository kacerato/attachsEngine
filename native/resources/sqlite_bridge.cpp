#include "resources/sqlite_bridge.h"

#include <sqlite3.h>

#include <cstring>

namespace {

// sqlite3_open passa o caminho como const char* terminado em nul — a fronteira C ABI de
// sqlite_bridge.h recebe (ponteiro, comprimento) por CONVENCOES.md §2, então esta função
// intermediária faz a única cópia com terminador nul necessária para chamar a API do SQLite.
// Usa um buffer de pilha pequeno com fallback de heap só para caminhos incomumente longos —
// caminhos de projeto na prática nunca chegam perto do limite de pilha razoável.
constexpr ae::i32 kStackPathBufferSize = 512;

} // namespace

extern "C" {

AetherSqliteConnection *AetherSqlite_Open(const char *utf8Path, ae::i32 pathLength) {
  AE_CHECK(utf8Path != nullptr, "AetherSqlite_Open: utf8Path nulo");
  AE_CHECK(pathLength >= 0, "AetherSqlite_Open: pathLength negativo");

  char stackBuffer[kStackPathBufferSize];
  char *heapBuffer = nullptr;
  char *nulTerminatedPath = stackBuffer;
  if (pathLength >= kStackPathBufferSize) {
    heapBuffer = new char[static_cast<ae::usize>(pathLength) + 1];
    nulTerminatedPath = heapBuffer;
  }
  std::memcpy(nulTerminatedPath, utf8Path, static_cast<ae::usize>(pathLength));
  nulTerminatedPath[pathLength] = '\0';

  sqlite3 *db = nullptr;
  int rc = sqlite3_open(nulTerminatedPath, &db);
  delete[] heapBuffer;

  if (rc != SQLITE_OK) {
    // sqlite3_open pode devolver um handle não-nulo mesmo em falha (para permitir ler a mensagem
    // de erro) — mas esta ABI simplifica para "nullptr == falha", então fecha e descarta aqui.
    if (db != nullptr) sqlite3_close(db);
    return nullptr;
  }
  return reinterpret_cast<AetherSqliteConnection *>(db);
}

AetherSqliteResult AetherSqlite_Close(AetherSqliteConnection *connection) {
  AE_CHECK(connection != nullptr, "AetherSqlite_Close: connection nula");
  int rc = sqlite3_close(reinterpret_cast<sqlite3 *>(connection));
  return rc == SQLITE_OK ? AetherSqliteResult::Ok
         : rc == SQLITE_BUSY ? AetherSqliteResult::Busy
                              : AetherSqliteResult::Error;
}

void AetherSqlite_GetLastError(AetherSqliteConnection *connection, const char **outUtf8Message, ae::i32 *outLength) {
  AE_CHECK(connection != nullptr, "AetherSqlite_GetLastError: connection nula");
  AE_CHECK(outUtf8Message != nullptr && outLength != nullptr, "AetherSqlite_GetLastError: ponteiro de saída nulo");

  const char *message = sqlite3_errmsg(reinterpret_cast<sqlite3 *>(connection));
  *outUtf8Message = message;
  *outLength = message != nullptr ? static_cast<ae::i32>(std::strlen(message)) : 0;
}

AetherSqliteStatement *AetherSqlite_Prepare(AetherSqliteConnection *connection, const char *utf8Sql, ae::i32 sqlLength) {
  AE_CHECK(connection != nullptr, "AetherSqlite_Prepare: connection nula");
  AE_CHECK(utf8Sql != nullptr, "AetherSqlite_Prepare: utf8Sql nulo");
  AE_CHECK(sqlLength >= 0, "AetherSqlite_Prepare: sqlLength negativo");

  sqlite3_stmt *stmt = nullptr;
  // sqlite3_prepare_v2 com nByte explícito NÃO exige terminação em nul (diferente de
  // sqlite3_open acima) — lê exatamente sqlLength bytes, então nenhuma cópia é necessária aqui.
  int rc = sqlite3_prepare_v2(reinterpret_cast<sqlite3 *>(connection), utf8Sql, sqlLength, &stmt, nullptr);
  if (rc != SQLITE_OK) return nullptr;
  return reinterpret_cast<AetherSqliteStatement *>(stmt);
}

void AetherSqlite_Finalize(AetherSqliteStatement *statement) {
  if (statement == nullptr) return; // mesma disciplina idempotente de sqlite3_finalize(nullptr), que já é no-op na API do SQLite
  sqlite3_finalize(reinterpret_cast<sqlite3_stmt *>(statement));
}

namespace {
AetherSqliteResult MapStepResult(int rc) {
  switch (rc) {
    case SQLITE_ROW: return AetherSqliteResult::Row;
    case SQLITE_DONE: return AetherSqliteResult::Done;
    case SQLITE_BUSY: return AetherSqliteResult::Busy;
    case SQLITE_MISUSE: return AetherSqliteResult::Misuse;
    default: return AetherSqliteResult::Error;
  }
}
} // namespace

AetherSqliteResult AetherSqlite_Step(AetherSqliteStatement *statement) {
  AE_CHECK(statement != nullptr, "AetherSqlite_Step: statement nulo");
  int rc = sqlite3_step(reinterpret_cast<sqlite3_stmt *>(statement));
  return MapStepResult(rc);
}

AetherSqliteResult AetherSqlite_Reset(AetherSqliteStatement *statement) {
  AE_CHECK(statement != nullptr, "AetherSqlite_Reset: statement nulo");
  auto *stmt = reinterpret_cast<sqlite3_stmt *>(statement);
  int rc = sqlite3_reset(stmt);
  if (rc != SQLITE_OK) return AetherSqliteResult::Error;
  rc = sqlite3_clear_bindings(stmt);
  return rc == SQLITE_OK ? AetherSqliteResult::Ok : AetherSqliteResult::Error;
}

AetherSqliteResult AetherSqlite_BindInt64(AetherSqliteStatement *statement, ae::i32 index, ae::i64 value) {
  AE_CHECK(statement != nullptr, "AetherSqlite_BindInt64: statement nulo");
  int rc = sqlite3_bind_int64(reinterpret_cast<sqlite3_stmt *>(statement), index, value);
  return rc == SQLITE_OK ? AetherSqliteResult::Ok : AetherSqliteResult::Error;
}

AetherSqliteResult AetherSqlite_BindDouble(AetherSqliteStatement *statement, ae::i32 index, double value) {
  AE_CHECK(statement != nullptr, "AetherSqlite_BindDouble: statement nulo");
  int rc = sqlite3_bind_double(reinterpret_cast<sqlite3_stmt *>(statement), index, value);
  return rc == SQLITE_OK ? AetherSqliteResult::Ok : AetherSqliteResult::Error;
}

AetherSqliteResult AetherSqlite_BindText(AetherSqliteStatement *statement, ae::i32 index, const char *utf8Value, ae::i32 length) {
  AE_CHECK(statement != nullptr, "AetherSqlite_BindText: statement nulo");
  AE_CHECK(length >= 0, "AetherSqlite_BindText: length negativo");
  // SQLITE_TRANSIENT: o SQLite copia o conteúdo internamente antes de retornar — necessário
  // porque o buffer gerenciado do chamador (um array C# atrás de um Span fixado) não tem
  // garantia de vida além da chamada P/Invoke atual.
  int rc = sqlite3_bind_text(reinterpret_cast<sqlite3_stmt *>(statement), index, utf8Value, length, SQLITE_TRANSIENT);
  return rc == SQLITE_OK ? AetherSqliteResult::Ok : AetherSqliteResult::Error;
}

AetherSqliteResult AetherSqlite_BindNull(AetherSqliteStatement *statement, ae::i32 index) {
  AE_CHECK(statement != nullptr, "AetherSqlite_BindNull: statement nulo");
  int rc = sqlite3_bind_null(reinterpret_cast<sqlite3_stmt *>(statement), index);
  return rc == SQLITE_OK ? AetherSqliteResult::Ok : AetherSqliteResult::Error;
}

AetherSqliteColumnType AetherSqlite_ColumnType(AetherSqliteStatement *statement, ae::i32 columnIndex) {
  AE_CHECK(statement != nullptr, "AetherSqlite_ColumnType: statement nulo");
  int type = sqlite3_column_type(reinterpret_cast<sqlite3_stmt *>(statement), columnIndex);
  switch (type) {
    case SQLITE_INTEGER: return AetherSqliteColumnType::Integer;
    case SQLITE_FLOAT: return AetherSqliteColumnType::Float;
    case SQLITE_TEXT: return AetherSqliteColumnType::Text;
    case SQLITE_BLOB: return AetherSqliteColumnType::Blob;
    default: return AetherSqliteColumnType::Null;
  }
}

ae::i64 AetherSqlite_ColumnInt64(AetherSqliteStatement *statement, ae::i32 columnIndex) {
  AE_CHECK(statement != nullptr, "AetherSqlite_ColumnInt64: statement nulo");
  return sqlite3_column_int64(reinterpret_cast<sqlite3_stmt *>(statement), columnIndex);
}

double AetherSqlite_ColumnDouble(AetherSqliteStatement *statement, ae::i32 columnIndex) {
  AE_CHECK(statement != nullptr, "AetherSqlite_ColumnDouble: statement nulo");
  return sqlite3_column_double(reinterpret_cast<sqlite3_stmt *>(statement), columnIndex);
}

void AetherSqlite_ColumnText(AetherSqliteStatement *statement, ae::i32 columnIndex, const char **outUtf8Text, ae::i32 *outLength) {
  AE_CHECK(statement != nullptr, "AetherSqlite_ColumnText: statement nulo");
  AE_CHECK(outUtf8Text != nullptr && outLength != nullptr, "AetherSqlite_ColumnText: ponteiro de saída nulo");

  auto *stmt = reinterpret_cast<sqlite3_stmt *>(statement);
  // sqlite3_column_bytes DEPOIS de sqlite3_column_text (não antes): a documentação do SQLite
  // exige essa ordem quando o valor pode precisar de conversão de tipo internamente — inverter a
  // ordem é um bug clássico e sutil de quem usa esta API pela primeira vez.
  const unsigned char *text = sqlite3_column_text(stmt, columnIndex);
  int length = sqlite3_column_bytes(stmt, columnIndex);
  *outUtf8Text = reinterpret_cast<const char *>(text);
  *outLength = length;
}

} // extern "C"
