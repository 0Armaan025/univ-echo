#pragma once

#include <sqlite3.h>

#include <string>

namespace univecho {
class Database {
public:
  explicit Database(const std::string &path);
  ~Database();

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  bool isOpen() const noexcept;

private:
  sqlite3 *db_ = nullptr;
};
}; // namespace univecho
