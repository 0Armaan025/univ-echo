#include "../../include/univ-echo/database.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <map>
#include <set>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include <iomanip>
#include <sstream>

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

// alright

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

    CREATE INDEX IF NOT EXISTS command_history_latest_idx
    ON command_history(
        command,
        (timestamp IS NOT NULL) DESC,
        timestamp DESC,
        id DESC
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
std::vector<CommandRecord> Database::searchCommands(const std::string &input) {

  if (input.empty()) {
    return {};
  }

  Statement statement(db_,
                      "SELECT h.id, h.command, h.source_shell, h.timestamp "
                      "FROM command_history AS h "
                      "WHERE h.command LIKE ? "
                      "AND h.id = ("
                      "   SELECT newer.id "
                      "   FROM command_history AS newer "
                      "   WHERE newer.command = h.command "
                      "   ORDER BY (newer.timestamp IS NOT NULL) DESC, "
                      "             newer.timestamp DESC, newer.id DESC "
                      "   LIMIT 1"
                      ") "
                      "ORDER BY (h.timestamp IS NOT NULL) DESC, "
                      "          h.timestamp DESC, h.id DESC "
                      "LIMIT 100;");

  const std::string pattern = "%" + input + "%";

  check(sqlite3_bind_text(statement.get(), 1, pattern.c_str(),
                          static_cast<int>(pattern.size()), SQLITE_TRANSIENT),
        db_, "Binding command search");

  std::vector<CommandRecord> results;
  int rc;

  while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
    CommandRecord record;

    record.id = sqlite3_column_int64(statement.get(), 0);
    record.command = columnText(statement.get(), 1);
    record.sourceShell = columnText(statement.get(), 2);

    if (sqlite3_column_type(statement.get(), 3) != SQLITE_NULL) {

      record.timestamp =
          static_cast<std::time_t>(sqlite3_column_int64(statement.get(), 3));
    }

    results.push_back(std::move(record));
  }

  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("Searching command hsitory: ") +
                             sqlite3_errmsg(db_));
  }

  return results;
}

std::vector<CommandRecord> Database::listCommands() {
  Statement statement(db_,
                      "SELECT h.id, h.command, h.source_shell, h.timestamp "
                      "FROM command_history AS h "
                      "WHERE h.id = ("
                      " SELECT newer.id FROM command_history AS newer "
                      " WHERE newer.command = h.command "
                      " ORDER BY (newer.timestamp IS NOT NULL) DESC, "
                      "           newer.timestamp DESC, newer.id DESC "
                      " LIMIT 1"
                      ") "
                      "ORDER BY (h.timestamp IS NOT NULL) DESC, "
                      "         h.timestamp DESC, h.id DESC;");

  std::vector<CommandRecord> results;
  int rc;
  // alright ig

  while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
    CommandRecord record;
    record.id = sqlite3_column_int64(statement.get(), 0);
    record.command = columnText(statement.get(), 1);
    record.sourceShell = columnText(statement.get(), 2);

    if (sqlite3_column_type(statement.get(), 3) != SQLITE_NULL) {
      record.timestamp =
          static_cast<std::time_t>(sqlite3_column_int64(statement.get(), 3));
    }

    results.push_back(std::move(record));
    ;
  }

  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("listing commands... ") +
                             sqlite3_errmsg(db_));
  }

  return results;
}
TerminalDNAStats Database::getTerminalDNA() {
  TerminalDNAStats stats;

  Statement statement(db_, "SELECT command, timestamp FROM command_history;");

  std::unordered_set<std::string> uniqueCommands;
  std::map<int, std::size_t> activeDays;
  std::map<std::string, std::size_t> toolCounts;
  std::map<std::string, std::size_t> categoryCounts;

  const auto identifyTool = [](const std::string &command) {
    const auto start = command.find_first_not_of(" \t");
    if (start == std::string::npos)
      return std::string{};

    const auto end = command.find_first_of(" \t", start);

    std::string executable = command.substr(
        start, end == std::string::npos ? std::string::npos : end - start);

    const auto slash = executable.find_last_of('/');
    if (slash != std::string::npos)
      executable = executable.substr(slash + 1);

    static const std::unordered_set<std::string> wrappers{"sudo", "command",
                                                          "time", "nohup"};

    if (wrappers.find(executable) != wrappers.end() &&
        end != std::string::npos) {

      const auto next = command.find_first_not_of(" \t", end);

      if (next != std::string::npos) {
        const auto nextEnd = command.find_first_of(" \t", next);

        executable = command.substr(next, nextEnd == std::string::npos
                                              ? std::string::npos
                                              : nextEnd - next);

        const auto nextSlash = executable.find_last_of('/');
        if (nextSlash != std::string::npos)
          executable = executable.substr(nextSlash + 1);
      }
    }

    return executable;
  };

  const auto identifyCategory = [](const std::string &tool) {
    static const std::unordered_set<std::string> development{
        "git",   "gcc",   "g++",   "clang",  "cmake",   "make",
        "ninja", "cargo", "rustc", "python", "python3", "node",
        "npm",   "npx",   "go",    "java"};

    static const std::unordered_set<std::string> navigation{
        "cd",     "ls",    "pwd",     "tree", "find",
        "locate", "which", "whereis", "file"};

    static const std::unordered_set<std::string> system{
        "systemctl", "journalctl", "ps",    "top",   "htop", "kill", "free",
        "uname",     "lscpu",      "lsblk", "mount", "df",   "du"};

    static const std::unordered_set<std::string> package{
        "pacman", "yay", "paru", "apt", "apt-get", "dnf", "flatpak", "snap"};

    static const std::unordered_set<std::string> network{
        "curl", "wget", "ping", "ssh", "scp", "ip", "nmcli", "nmap"};

    static const std::unordered_set<std::string> editor{
        "nvim", "vim", "vi", "nano", "emacs", "code"};

    if (development.find(tool) != development.end())
      return "Development";
    if (navigation.find(tool) != navigation.end())
      return "Navigation";
    if (system.find(tool) != system.end())
      return "System";
    if (package.find(tool) != package.end())
      return "Packages";
    if (network.find(tool) != network.end())
      return "Networking";
    if (editor.find(tool) != editor.end())
      return "Editors";

    return "Other";
  };

  sqlite3_stmt *stmt = statement.get();
  int rc = SQLITE_OK;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    ++stats.totalEntries;

    const std::string command = columnText(stmt, 0);
    uniqueCommands.insert(command);

    const std::string tool = identifyTool(command);

    if (!tool.empty()) {
      ++toolCounts[tool];
      ++categoryCounts[identifyCategory(tool)];
    }

    if (sqlite3_column_type(stmt, 1) == SQLITE_NULL)
      continue;

    const auto timestamp =
        static_cast<std::time_t>(sqlite3_column_int64(stmt, 1));

    std::tm local{};

#ifdef _WIN32
    const bool timeValid = localtime_s(&local, &timestamp) == 0;
#else
    const bool timeValid = localtime_r(&timestamp, &local) != nullptr;
#endif

    if (!timeValid)
      continue;

    ++stats.hourlyActivity[local.tm_hour];

    const int dateKey = (local.tm_year + 1900) * 10000 +
                        (local.tm_mon + 1) * 100 + local.tm_mday;

    ++activeDays[dateKey];
  }

  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("Reading terminal DNA: ") +
                             sqlite3_errmsg(db_));
  }

  stats.uniqueCommands = uniqueCommands.size();
  stats.activeDays = activeDays.size();

  for (const auto &[date, uses] : activeDays) {
    if (uses > 0 &&
        uses > activeDays[stats.mostActiveDate.empty() ? date : date]) {
      // The actual most-active date is selected below.
    }
  }

  int bestDate = 0;
  std::size_t bestDateUses = 0;

  for (const auto &[date, uses] : activeDays) {
    if (uses > bestDateUses) {
      bestDate = date;
      bestDateUses = uses;
    }
  }

  if (bestDate != 0) {
    const int year = bestDate / 10000;
    const int month = (bestDate / 100) % 100;
    const int day = bestDate % 100;

    std::ostringstream dateText;
    dateText << year << '-' << std::setfill('0') << std::setw(2) << month << '-'
             << std::setw(2) << day;

    stats.mostActiveDate = dateText.str();
  }

  for (int hour = 0; hour < 24; ++hour) {
    if (stats.hourlyActivity[hour] > stats.peakHourUses) {
      stats.peakHour = hour;
      stats.peakHourUses = stats.hourlyActivity[hour];
    }
  }

  for (const auto &[name, uses] : toolCounts)
    stats.topTools.push_back({name, uses});

  for (const auto &[name, uses] : categoryCounts)
    stats.categories.push_back({name, uses});

  const auto sortByUses = [](const auto &a, const auto &b) {
    return a.uses > b.uses;
  };

  std::sort(stats.topTools.begin(), stats.topTools.end(), sortByUses);
  std::sort(stats.categories.begin(), stats.categories.end(), sortByUses);

  if (stats.topTools.size() > 6)
    stats.topTools.resize(6);

  if (stats.categories.size() > 6)
    stats.categories.resize(6);

  return stats;
}

} // namespace univecho
