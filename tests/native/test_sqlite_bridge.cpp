// Testes de correção da fronteira C ABI sobre o SQLite vendorizado
// (native/resources/sqlite_bridge.h/.cpp — item 1.4.5 do plano).
//
// Cada teste abre um banco em arquivo temporário real (não :memory:) para
// exercitar o mesmo caminho que o editor usa de verdade — abrir um arquivo
// físico é onde bugs de permissão/path costumam aparecer, um banco em memória
// mascararia isso.
#include "harness.h"
#include "resources/sqlite_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace ae::test;

namespace {

// Caminho de arquivo temporário único por teste — mesmo padrão de
// NovoCaminhoTemporario() do lado C# (tests/Aether.Tests/EditingTests.cs),
// reimplementado aqui porque os dois runners de teste (nativo e gerenciado)
// são deliberadamente independentes, sem código compartilhado entre eles.
std::string NovoCaminhoTemporario(const char *sufixo) {
  static int contador = 0;
  const char *tempDir = std::getenv("TEMP");
  if (tempDir == nullptr) tempDir = std::getenv("TMPDIR");
  if (tempDir == nullptr) tempDir = "/tmp";
  return std::string(tempDir) + "/aether-sqlite-test-" + std::to_string(++contador) + "-" + sufixo + ".db";
}

} // namespace

AE_TEST(open_e_close_em_arquivo_novo_sucede_sem_deixar_estado_pendurado) {
  std::string caminho = NovoCaminhoTemporario("open-close");
  std::remove(caminho.c_str());

  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));
  AE_EXPECT_TRUE(conn != nullptr, "Open deve suceder para um caminho de arquivo válido e gravável");

  AetherSqliteResult result = AetherSqlite_Close(conn);
  AE_EXPECT_TRUE(result == AetherSqliteResult::Ok, "Close numa conexão sem statement pendente deve suceder");

  std::remove(caminho.c_str());
}

AE_TEST(open_em_diretorio_inexistente_devolve_nullptr) {
  const char path[] = "/diretorio/que/nao/existe/nem/vai/existir/teste.db";
  AetherSqliteConnection *conn = AetherSqlite_Open(path, static_cast<ae::i32>(sizeof(path) - 1));
  AE_EXPECT_TRUE(conn == nullptr, "Open num diretório pai inexistente deve falhar explicitamente, não criar silenciosamente");
}

AE_TEST(prepare_com_sql_invalido_devolve_nullptr_e_registra_erro) {
  std::string caminho = NovoCaminhoTemporario("prepare-invalido");
  std::remove(caminho.c_str());
  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));
  AE_EXPECT_TRUE(conn != nullptr, "setup: Open deve suceder");

  const char sqlInvalido[] = "ISTO NAO E SQL VALIDO ;;;";
  AetherSqliteStatement *stmt = AetherSqlite_Prepare(conn, sqlInvalido, static_cast<ae::i32>(sizeof(sqlInvalido) - 1));
  AE_EXPECT_TRUE(stmt == nullptr, "SQL sintaticamente inválido deve falhar no Prepare, não no Step");

  const char *errMsg = nullptr;
  ae::i32 errLen = 0;
  AetherSqlite_GetLastError(conn, &errMsg, &errLen);
  AE_EXPECT_TRUE(errLen > 0, "erro de sintaxe deve deixar uma mensagem não-vazia recuperável");

  AetherSqlite_Close(conn);
  std::remove(caminho.c_str());
}

AE_TEST(create_table_insert_select_roundtrip_preserva_valores_dos_tres_tipos) {
  std::string caminho = NovoCaminhoTemporario("roundtrip");
  std::remove(caminho.c_str());
  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));
  AE_EXPECT_TRUE(conn != nullptr, "setup: Open deve suceder");

  const char createSql[] = "CREATE TABLE item (id INTEGER PRIMARY KEY, nome TEXT, peso REAL)";
  AetherSqliteStatement *createStmt = AetherSqlite_Prepare(conn, createSql, static_cast<ae::i32>(sizeof(createSql) - 1));
  AE_EXPECT_TRUE(createStmt != nullptr, "CREATE TABLE deve preparar com sucesso");
  AE_EXPECT_TRUE(AetherSqlite_Step(createStmt) == AetherSqliteResult::Done, "CREATE TABLE não produz linhas, só Done");
  AetherSqlite_Finalize(createStmt);

  const char insertSql[] = "INSERT INTO item (id, nome, peso) VALUES (?, ?, ?)";
  AetherSqliteStatement *insertStmt = AetherSqlite_Prepare(conn, insertSql, static_cast<ae::i32>(sizeof(insertSql) - 1));
  AE_EXPECT_TRUE(insertStmt != nullptr, "INSERT parametrizado deve preparar com sucesso");

  const char nome[] = "espada-de-ferro";
  AE_EXPECT_TRUE(AetherSqlite_BindInt64(insertStmt, 1, 42) == AetherSqliteResult::Ok, "bind do parâmetro inteiro deve suceder");
  AE_EXPECT_TRUE(AetherSqlite_BindText(insertStmt, 2, nome, static_cast<ae::i32>(sizeof(nome) - 1)) == AetherSqliteResult::Ok, "bind do parâmetro texto deve suceder");
  AE_EXPECT_TRUE(AetherSqlite_BindDouble(insertStmt, 3, 3.5) == AetherSqliteResult::Ok, "bind do parâmetro real deve suceder");
  AE_EXPECT_TRUE(AetherSqlite_Step(insertStmt) == AetherSqliteResult::Done, "INSERT bem-sucedido não produz linhas, só Done");
  AetherSqlite_Finalize(insertStmt);

  const char selectSql[] = "SELECT id, nome, peso FROM item WHERE id = ?";
  AetherSqliteStatement *selectStmt = AetherSqlite_Prepare(conn, selectSql, static_cast<ae::i32>(sizeof(selectSql) - 1));
  AE_EXPECT_TRUE(selectStmt != nullptr, "SELECT parametrizado deve preparar com sucesso");
  AE_EXPECT_TRUE(AetherSqlite_BindInt64(selectStmt, 1, 42) == AetherSqliteResult::Ok, "bind do filtro deve suceder");

  AE_EXPECT_TRUE(AetherSqlite_Step(selectStmt) == AetherSqliteResult::Row, "linha inserida deve ser encontrada pelo SELECT");
  AE_EXPECT_TRUE(AetherSqlite_ColumnType(selectStmt, 0) == AetherSqliteColumnType::Integer, "coluna id deve reportar tipo Integer");
  AE_EXPECT_EQ(AetherSqlite_ColumnInt64(selectStmt, 0), static_cast<ae::i64>(42), "id lido deve bater com o id inserido");

  const char *nomeUtf8 = nullptr;
  ae::i32 nomeLen = 0;
  AetherSqlite_ColumnText(selectStmt, 1, &nomeUtf8, &nomeLen);
  std::string nomeLido(nomeUtf8, static_cast<size_t>(nomeLen));
  AE_EXPECT_TRUE(nomeLido == "espada-de-ferro", "texto lido deve bater exatamente com o texto inserido, incluindo hífens");

  double peso = AetherSqlite_ColumnDouble(selectStmt, 2);
  AE_EXPECT_TRUE(peso > 3.4999 && peso < 3.5001, "peso lido deve bater com o peso inserido");

  AE_EXPECT_TRUE(AetherSqlite_Step(selectStmt) == AetherSqliteResult::Done, "só uma linha foi inserida, segundo Step deve esgotar o resultado");
  AetherSqlite_Finalize(selectStmt);

  AetherSqlite_Close(conn);
  std::remove(caminho.c_str());
}

AE_TEST(select_sem_linhas_correspondentes_devolve_done_na_primeira_chamada) {
  std::string caminho = NovoCaminhoTemporario("sem-linhas");
  std::remove(caminho.c_str());
  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));

  const char createSql[] = "CREATE TABLE vazio (id INTEGER)";
  AetherSqliteStatement *createStmt = AetherSqlite_Prepare(conn, createSql, static_cast<ae::i32>(sizeof(createSql) - 1));
  AetherSqlite_Step(createStmt);
  AetherSqlite_Finalize(createStmt);

  const char selectSql[] = "SELECT id FROM vazio";
  AetherSqliteStatement *selectStmt = AetherSqlite_Prepare(conn, selectSql, static_cast<ae::i32>(sizeof(selectSql) - 1));
  AE_EXPECT_TRUE(AetherSqlite_Step(selectStmt) == AetherSqliteResult::Done, "tabela vazia deve produzir Done imediatamente, nunca Row");
  AetherSqlite_Finalize(selectStmt);

  AetherSqlite_Close(conn);
  std::remove(caminho.c_str());
}

AE_TEST(bind_null_e_lido_de_volta_como_tipo_null) {
  std::string caminho = NovoCaminhoTemporario("bind-null");
  std::remove(caminho.c_str());
  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));

  const char createSql[] = "CREATE TABLE t (v TEXT)";
  AetherSqliteStatement *createStmt = AetherSqlite_Prepare(conn, createSql, static_cast<ae::i32>(sizeof(createSql) - 1));
  AetherSqlite_Step(createStmt);
  AetherSqlite_Finalize(createStmt);

  const char insertSql[] = "INSERT INTO t (v) VALUES (?)";
  AetherSqliteStatement *insertStmt = AetherSqlite_Prepare(conn, insertSql, static_cast<ae::i32>(sizeof(insertSql) - 1));
  AE_EXPECT_TRUE(AetherSqlite_BindNull(insertStmt, 1) == AetherSqliteResult::Ok, "bind de NULL deve suceder");
  AetherSqlite_Step(insertStmt);
  AetherSqlite_Finalize(insertStmt);

  const char selectSql[] = "SELECT v FROM t";
  AetherSqliteStatement *selectStmt = AetherSqlite_Prepare(conn, selectSql, static_cast<ae::i32>(sizeof(selectSql) - 1));
  AetherSqlite_Step(selectStmt);
  AE_EXPECT_TRUE(AetherSqlite_ColumnType(selectStmt, 0) == AetherSqliteColumnType::Null, "valor bindado como NULL deve ser lido de volta como tipo Null");
  AetherSqlite_Finalize(selectStmt);

  AetherSqlite_Close(conn);
  std::remove(caminho.c_str());
}

AE_TEST(reset_permite_reexecutar_o_mesmo_statement_com_bindings_diferentes) {
  std::string caminho = NovoCaminhoTemporario("reset");
  std::remove(caminho.c_str());
  AetherSqliteConnection *conn = AetherSqlite_Open(caminho.c_str(), static_cast<ae::i32>(caminho.size()));

  const char createSql[] = "CREATE TABLE t (v INTEGER)";
  AetherSqliteStatement *createStmt = AetherSqlite_Prepare(conn, createSql, static_cast<ae::i32>(sizeof(createSql) - 1));
  AetherSqlite_Step(createStmt);
  AetherSqlite_Finalize(createStmt);

  const char insertSql[] = "INSERT INTO t (v) VALUES (?)";
  AetherSqliteStatement *insertStmt = AetherSqlite_Prepare(conn, insertSql, static_cast<ae::i32>(sizeof(insertSql) - 1));

  AetherSqlite_BindInt64(insertStmt, 1, 1);
  AE_EXPECT_TRUE(AetherSqlite_Step(insertStmt) == AetherSqliteResult::Done, "primeira execução deve suceder");
  AE_EXPECT_TRUE(AetherSqlite_Reset(insertStmt) == AetherSqliteResult::Ok, "Reset deve suceder entre execuções");
  AetherSqlite_BindInt64(insertStmt, 1, 2);
  AE_EXPECT_TRUE(AetherSqlite_Step(insertStmt) == AetherSqliteResult::Done, "segunda execução após Reset deve suceder com o novo bind");
  AetherSqlite_Finalize(insertStmt);

  const char countSql[] = "SELECT COUNT(*) FROM t";
  AetherSqliteStatement *countStmt = AetherSqlite_Prepare(conn, countSql, static_cast<ae::i32>(sizeof(countSql) - 1));
  AetherSqlite_Step(countStmt);
  AE_EXPECT_EQ(AetherSqlite_ColumnInt64(countStmt, 0), static_cast<ae::i64>(2), "duas execuções via Reset devem ter inserido duas linhas, não uma sobrescrevendo a outra");
  AetherSqlite_Finalize(countStmt);

  AetherSqlite_Close(conn);
  std::remove(caminho.c_str());
}
