#pragma once

#include <sqlite3.h>
#include <vector>

#include "./command_history.hpp"
#include <ctime>
#include <optional>
#include <string>

#include <iostream>
using namespace std;
namespace univecho {

struct Memory {
  long long id{};
  string title;
  std::string content;
  std::string createdAt;
};

struct CommandRecord {
  long long id{};
  std::string command;

  std::string sourceShell;

  std::optional<std::time_t> timestamp;
};

class Database {
public:
  explicit Database(const std::string &path);
  ~Database();

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  void add(const std::string &title, const std::string &content);

  // ik i have using namespace std and iostream included but just smhw using
  // std:: is a vibe

  std::vector<Memory> list();

  std::vector<Memory> search(const std::string &query);

  void importCommands(std::vector<CommandHistoryEntry> &entries);
  std::vector<CommandRecord> searchCommands(const std::string &query);
  std::vector<CommandRecord> listCommands();

private:
  sqlite3 *db_ = nullptr;

  void initialize();
};
}; // namespace univecho
