#include "../../include/univ-echo/database.hpp"
#include <sqlite3.h>
#include <stdexcept>

#include <iostream>
using namespace std;
namespace univecho {
Database::Database(const std::string &path) {
  const int result = sqlite3_open(path.c_str(), &db_);

  if (result != SQLITE_OK) {
    const std::string error = db_ ? sqlite3_errmsg(db_) : "Allocation failure!";

    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }

    throw std::runtime_error("Couldn't open UNIVECHO database: " + error);
  }

  const char *schema = R"(
  CREATE TABLE IF NOT EXISTS memories (
      id INTEGER PRIMARY KEY,
      title TEXT NOT NULL,
      content TEXT NOT NULL DEFAULT '',
      created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
  );
  )";

  char *error = nullptr;

  const int schemaResult = sqlite3_exec(db_, schema, nullptr, nullptr, &error);

  if (schemaResult != SQLITE_OK) {
    const std::string msg = error ? error : "Unknown schema error";

    sqlite3_free(error);
    sqlite3_close(db_);
    db_ = nullptr;

    throw std::runtime_error("could not init the memory storage. " + msg);
  }
}

Database::~Database() {
  if (db_) {
    sqlite3_close(db_);
  }
}

bool Database::isOpen() const noexcept { return db_ != nullptr; }
} // namespace univecho
