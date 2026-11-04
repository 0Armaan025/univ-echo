#pragma once

#include <sqlite3.h>
#include <vector>

#include "./command_history.hpp"
#include <array>
#include <ctime>
#include <optional>
#include <string>
#include <utility>

#include <iostream>
namespace univecho {

struct Memory {
  long long id{};
  std::string title;
  std::string content;
  std::string createdAt;
};

struct CommandRecord {
  long long id{};
  std::string command;

  std::string sourceShell;

  std::optional<std::time_t> timestamp;

  float semanticScore{};
  float semanticSimilarity{};
  float lexicalScore{};
};

struct TerminalDNATool {
  std::string name;
  std::size_t uses{};
};

struct TerminalDNACategory {
  std::string name;
  std::size_t uses{};
};
struct TerminalDNAStats {

  std::size_t totalEntries{};
  std::size_t uniqueCommands{};
  std::size_t activeDays{};

  std::string mostActiveDate;
  int peakHour{-1};
  std::size_t peakHourUses{};

  std::array<std::size_t, 24> hourlyActivity{};
  std::vector<TerminalDNATool> topTools;

  std::vector<TerminalDNACategory> categories;
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
  TerminalDNAStats getTerminalDNA();

private:
  sqlite3 *db_ = nullptr;

  void initialize();
};
}; // namespace univecho
