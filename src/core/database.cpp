#include "../../include/univ-echo/database.hpp"
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <string>

#include <iostream>
using namespace std;
namespace univecho {
namespace {
void check(int result, sqlite3 *db, const std::string &action) {
  if (result != SQLITE_OK) {
    throw std::runtime_error(action + ": " + sqlite3_errmsg(db));
  }
}

class Statement {
public:
  Statement(sqlite3 *db, const char *sql) : db_(db) {
    check(sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr), db,

          "preparing sql");
  }

  ~Statement() { sqlite3_finalize(stmt_); }

  sqlite3_stmt *get() const { return stmt_; };

private:
  sqlite3 *db_;
  sqlite3_stmt *stmt_ = nullptr;
};

std::string columnText(sqlite3_stmt *stmt, int column) {
  const auto *value = sqlite3_column_text(stmt, column);

  return value ? reinterpret_cast<const char *>(value) : "";
}

// quoting each word and fts5 query syntax, all words must match.

std::string makeQuery(const std::string &input) {
  std::istringstream stream(input);
  std::string word;
  std::string query;

  while (stream >> word) {
    std::string quoted = "\"";

    for (char c : word) {
      if (c == '"') {

        quoted += "\"\"";
      } else {
        quoted += c;
      }
    }

    quoted = quoted + '"';

    if (!query.empty()) {
      query += " AND ";
    }

    query += quoted;
  }

  return query;
}

std::vector<Memory> readMemories(sqlite3 *db, const char *sql,
                                 const std::string *query = nullptr) {
  Statement statement(db, sql);
  sqlite3_stmt *stmt = statement.get();

  if (query) {
    check(sqlite3_bind_text(stmt, 1, query->c_str(),
                            static_cast<int>(query->size()), SQLITE_TRANSIENT),
          db, "Binding search query");
  }

  std::vector<Memory> results;
  int rc;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back({sqlite3_column_int64(stmt, 0), columnText(stmt, 1),
                       columnText(stmt, 2), columnText(stmt, 3)});
  }
  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("Reading memories: ") +
                             sqlite3_errmsg(db));
  }

  return results;
}

} // namespace
//
//

Database::Database(const std::string &path) {
  const int rc = sqlite3_open(path.c_str(), &db_);

  if (rc != SQLITE_OK) {
    std::string error =
        db_ ? sqlite3_errmsg(db_) : "Couldn't allocate SQLite connection";

    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }

    throw std::runtime_error("Opening database: " + error);
  }

  try {
    initialize();
  } catch (...) {
    sqlite3_close(db_);
    db_ = nullptr;
    throw;
  }
}

Database::~Database() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void Database::initialize() {

  const char *schema = R"SQL(
    CREATE TABLE IF NOT EXISTS memories (
        id INTEGER PRIMARY KEY,
        title TEXT NOT NULL,
        content TEXT NOT NULL DEFAULT '',
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE VIRTUAL TABLE IF NOT EXISTS memories_fts
    USING fts5(
        title,
        content,
        content='memories',
        content_rowid='id'
    );

    CREATE TRIGGER IF NOT EXISTS memories_ai
    AFTER INSERT ON memories BEGIN
        INSERT INTO memories_fts(rowid, title, content)
        VALUES (new.id, new.title, new.content);
    END;

    CREATE TRIGGER IF NOT EXISTS memories_ad
    AFTER DELETE ON memories BEGIN
        INSERT INTO memories_fts(
            memories_fts, rowid, title, content
        ) VALUES (
            'delete', old.id, old.title, old.content
        );
    END;

    CREATE TRIGGER IF NOT EXISTS memories_au
    AFTER UPDATE ON memories BEGIN
        INSERT INTO memories_fts(
            memories_fts, rowid, title, content
        ) VALUES (
            'delete', old.id, old.title, old.content
        );

        INSERT INTO memories_fts(rowid, title, content)
        VALUES (new.id, new.title, new.content);
    END;

    CREATE TABLE IF NOT EXISTS command_history (
        id INTEGER PRIMARY KEY,
        command TEXT NOT NULL,
        source_shell TEXT NOT NULL,
        timestamp INTEGER,
        imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        UNIQUE(command, source_shell, timestamp)
    );
)SQL";

  char *error = nullptr;
  const int rc = sqlite3_exec(db_, schema, nullptr, nullptr, &error);

  if (rc != SQLITE_OK) {
    const std::string message = error ? error : sqlite3_errmsg(db_);
    sqlite3_free(error);

    throw std::runtime_error("Initialzing database/FTS5: " + message);
  }

  // populate index

  char *rebuildError = nullptr;

  const int rebuildRc = sqlite3_exec(
      db_, "INSERT INTO memories_fts(memories_fts) VALUES ('rebuild');",
      nullptr, nullptr, &rebuildError);

  if (rebuildRc != SQLITE_OK) {
    const std::string message =
        rebuildError ? rebuildError : sqlite3_errmsg(db_);
    sqlite3_free(rebuildError);

    throw std::runtime_error("Building search index; " + message);
  }
}

void Database::add(const std::string &title, const std::string &content) {
  Statement statement(db_, "INSERT INTO memories(title, content) VALUES(?,?);");

  check(sqlite3_bind_text(statement.get(), 1, title.c_str(),
                          static_cast<int>(title.size()), SQLITE_TRANSIENT),
        db_, "Binding title");

  check(sqlite3_bind_text(statement.get(), 2, content.c_str(),
                          static_cast<int>(content.size()), SQLITE_TRANSIENT),
        db_, "Binding content");

  const int rc = sqlite3_step(statement.get());

  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("Saving memory: ") +
                             sqlite3_errmsg(db_));
  }
}

std::vector<Memory> Database::list() {
  return readMemories(db_, "SELECT id, title, content, created_at "
                           "FROM memories ORDER BY id DESC LIMIT 200;");
}

std::vector<Memory> Database::search(const std::string &input) {

  const std::string query = makeQuery(input);

  if (query.empty()) {
    return {};
  }

  return readMemories(db_,
                      "SELECT m.id, m.title, m.content, m.created_at "
                      "FROM memories_fts "
                      "JOIN memories AS m ON m.id = memories_fts.rowid "
                      "WHERE memories_fts MATCH ?"
                      "ORDER BY rank LIMIT 100;",
                      &query);
}

void Database::importCommands(std::vector<CommandHistoryEntry> &entries) {
  const char *sql = R"SQL(
    INSERT INTO command_history(command, source_shell, timestamp)
    VALUES (?, ?, ?)
    ON CONFLICT(command, source_shell, timestamp) DO NOTHING;
  )SQL";

  Statement statement(db_, sql);

  for (const auto &entry : entries) {
    check(sqlite3_bind_text(statement.get(), 1, entry.command.c_str(),
                            static_cast<int>(entry.command.size()),
                            SQLITE_TRANSIENT),
          db_, "Binding command");
    check(sqlite3_bind_text(statement.get(), 2, entry.sourceShell.c_str(),
                            static_cast<int>(entry.sourceShell.size()),
                            SQLITE_TRANSIENT),
          db_, "Binding source shell");

    if (entry.timestamp) {
      check(sqlite3_bind_int64(statement.get(), 3,
                               static_cast<sqlite3_int64>(*entry.timestamp)),
            db_, "Binding tiemstamp");
    } else {
      check(sqlite3_bind_null(statement.get(), 3), db_, "Binding timestamp");
    }

    const int rc = sqlite3_step(statement.get());

    if (rc != SQLITE_DONE) {
      throw std::runtime_error(std::string("Importing command history: ") +
                               sqlite3_errmsg(db_));
    }

    // reusing

    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
  }
}

} // namespace univecho
