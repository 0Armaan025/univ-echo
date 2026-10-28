#include "../include/univ-echo/command_history.hpp"
#include "../include/univ-echo/database.hpp"
#include "../include/univ-echo/semantic_search.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <cstdio>
#include <ctime>
#include <iomanip>

#include <vector>

namespace {
std::string databasePath() {
  const char *home = std::getenv("HOME");

  if (!home || !*home) {
    throw std::runtime_error("HOME env variable is missing!!!!");
  }

  const auto directory = std::filesystem::path(home) / ".local/share/univecho";

  std::filesystem::create_directories(directory);
  return (directory / "univecho.db").string();
}

std::string joinArgs(int argc, char *argv[], int start) {
  std::string result;

  for (int i = start; i < argc; ++i) {
    if (!result.empty())
      result += ' ';
    result += argv[i];
  }

  return result;
}

std::string prompt(const std::string &label) {
  char buffer[1024]{};

  clear();

  mvaddstr(1, 2, label.c_str());
  mvaddstr(3, 2, "> ");
  refresh();

  echo();
  curs_set(1);
  getnstr(buffer, static_cast<int>(sizeof(buffer) - 1));
  noecho();
  curs_set(0);
  return buffer;
}

bool copyToClipboard(const std::string &command) {
  FILE *pipe = popen("wl-copy", "w");

  if (!pipe) {
    return false;
  }

  const bool written =
      std::fwrite(command.data(), 1, command.size(), pipe) == command.size();

  return pclose(pipe) == 0 && written;
}

void showMemory(const univecho::Memory &memory) {
  clear();
  mvprintw(1, 2, "MEMORY #%lld", memory.id);
  mvprintw(3, 2, "Title: %s", memory.title.c_str());
  mvprintw(4, 2, "Created: %s", memory.createdAt.c_str());
  mvprintw(6, 2, "Content: ");

  int row = 7;
  std::string line;
  const int maxRows = std::max(8, LINES - 3);

  for (char c : memory.content) {
    if (c == '\n' || line.size() >= 120) {
      if (row >= maxRows)
        break;
      mvaddnstr(row++, 2, line.c_str(), COLS > 4 ? COLS - 4 : 1);
      line.clear();

      if (c == '\n')
        continue;
    }

    if (c != '\r')
      line += c;
  }

  if (!line.empty() && row < maxRows) {
    mvaddnstr(row, 2, line.c_str(), COLS > 4 ? COLS - 4 : 1);
  }

  mvaddstr(LINES - 1, 2, "Press any key to return!!!");

  refresh();
  getch();
  // idk what this will do lollll
}

void draw(const std::vector<univecho::Memory> &memories, int selected,
          const std::string &mode) {
  erase();

  mvaddstr(0, 2, " U N I V E C H O   |    UNIVERSAL MEMORY");
  mvprintw(1, 2, "Mode: %s | %zu results", mode.c_str(), memories.size());

  mvaddstr(3, 2, "YOUR MEMORIES");

  for (std::size_t i = 0; i < memories.size(); ++i) {
    const auto &memory = memories[static_cast<std::size_t>(i)];
    const int row = 5 + i;

    if (static_cast<int>(i) == selected) {
      attron(A_REVERSE);
    }

    mvprintw(row, 2, "%c #%lld  ", static_cast<int>(i) == selected ? '>' : ' ',
             memory.id);

    const int titleColumn = 2 + 4 + 20;
    mvaddnstr(row, titleColumn, memory.title.c_str(),
              std::max(1, COLS - titleColumn - 2));

    if (static_cast<int>(i) == selected) {
      attroff(A_REVERSE);
    }
  }

  mvaddstr(LINES - 3, 2,
           "[n] New [/] Search/Ask [c] Commands [r] All [Enter] Open");
  mvaddstr(LINES - 2, 2, "[Up/Down] Navigate    [q] Quit");

  refresh();
}

void drawCommands(const std::vector<univecho::CommandRecord> &records,
                  int selected, const std::string &query) {

  erase();

  mvaddstr(0, 2, "   UNIVECHO   |   COMMAND HISTORY");
  mvprintw(1, 2, "Search: %s | %zu results", query.c_str(), records.size());

  if (COLS < 45 || LINES < 12) {
    mvaddstr(3, 2, "Terminal too small for the command table...");
    mvaddstr(LINES - 2, 2, "Press esc to return");

    refresh();

    return;
  }

  const int tableX = 2;
  const int tableWidth = COLS - 4;
  const int timeWidth = 19;
  const int shellWidth = 6;

  const int scoreWidth = 7;
  const int commandWidth = tableWidth - timeWidth - shellWidth - scoreWidth - 4;

  const int timeX = 3;
  const int commandX = timeX + timeWidth + 1;
  const int shellX = commandX + commandWidth + 1;
  const int scoreX = shellX + shellWidth + 1;

  const int topY = 3;
  const int headerY = 4;
  const int separatorY = 5;
  const int firstRowY = 6;
  const int visibleRows = LINES - 11;

  // top border

  mvaddch(topY, tableX, ACS_ULCORNER);
  mvhline(topY, tableX + 1, ACS_HLINE, tableWidth - 2);
  mvaddch(topY, tableX + tableWidth - 1, ACS_URCORNER);

  // header

  attron(A_BOLD);
  mvaddnstr(headerY, timeX, "TIME", timeWidth);
  mvaddnstr(headerY, commandX, "COMMAND", commandWidth);
  mvaddnstr(headerY, shellX, "SHELL", shellWidth);
  mvaddnstr(headerY, scoreX, "SCORE", scoreWidth);

  attroff(A_BOLD);

  // header separator

  mvaddch(separatorY, tableX, ACS_LTEE);
  mvhline(separatorY, tableX + 1, ACS_HLINE, tableWidth - 2);

  mvaddch(separatorY, tableX + tableWidth - 1, ACS_RTEE);

  for (int i = 0; i < visibleRows; ++i) {
    const int index = selected - selected % visibleRows + i;

    const int row = firstRowY + i;

    if (index >= static_cast<int>(records.size())) {
      break;
    }

    const auto &record = records[static_cast<std::size_t>(index)];

    if (index == selected) {
      attron(A_REVERSE);
    }

    std::string timestampText;

    if (record.timestamp) {
      const std::time_t timestamp = *record.timestamp;
      std::tm localTime{};

      if (localtime_r(&timestamp, &localTime)) {
        char buffer[32]{};

        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                          &localTime) > 0) {

          timestampText = buffer;
        }
      }
    }

    const int scorePercent =
        static_cast<int>(std::clamp(record.semanticScore, 0.0F, 1.0F) * 100.0F);

    mvaddnstr(row, timeX, timestampText.c_str(), timeWidth);
    mvaddnstr(row, commandX, record.command.c_str(), commandWidth);
    mvaddnstr(row, shellX, record.sourceShell.c_str(), shellWidth);
    mvprintw(row, scoreX, "%3d%%", scorePercent);

    if (index == selected) {
      attroff(A_REVERSE);
    }
  }

  mvvline(headerY, tableX, ACS_VLINE, firstRowY + visibleRows - headerY);
  mvvline(headerY, timeX + timeWidth, ACS_VLINE,
          firstRowY + visibleRows - headerY);
  mvvline(headerY, commandX + commandWidth, ACS_VLINE,
          firstRowY + visibleRows - headerY);
  mvvline(headerY, tableX + tableWidth - 1, ACS_VLINE,
          firstRowY + visibleRows - headerY);

  mvvline(headerY, shellX + shellWidth, ACS_VLINE,
          firstRowY + visibleRows - headerY);

  mvaddch(separatorY, timeX + timeWidth, ACS_PLUS);
  mvaddch(separatorY, commandX + commandWidth, ACS_PLUS);
  mvaddch(separatorY, shellX + shellWidth, ACS_PLUS);

  const int bottomY = firstRowY + visibleRows;

  mvaddch(bottomY, tableX, ACS_LLCORNER);
  mvhline(bottomY, tableX + 1, ACS_HLINE, tableWidth - 2);
  mvaddch(bottomY, tableX + tableWidth - 1, ACS_LRCORNER);

  mvaddstr(LINES - 3, 2,
           "[Up/Down] Navigate  [Enter] Copy   [/] Search   [Esc] Back");
  mvaddstr(LINES - 2, 2, "[q] Back");
  refresh();
}

std::vector<univecho::CommandRecord>
searchWithLoading(univecho::Database &database, const std::string &query) {
  erase();
  mvaddstr(1, 2, "UNIVECHO | SEMANTIC COMMAND SEARCH");
  mvaddstr(3, 2, "Loading model results...");
  mvaddstr(4, 2, "Embedding commands and ranking matches.");
  refresh();

  return univecho::semanticSearch(database, query);
}

void runCommandHistory(univecho::Database &database) {
  std::string query = prompt("Search command history");
  auto records = searchWithLoading(database, query);
  int selected = 0;

  while (true) {
    if (!records.empty()) {
      selected = std::clamp(selected, 0, static_cast<int>(records.size()) - 1);

    } else {
      selected = 0;
    }

    drawCommands(records, selected, query);

    const int key = getch();

    switch (key) {
    case 27:

    case 'q':
      return;
    case KEY_UP:
      if (selected > 0) {
        --selected;
      }
      break;

    case KEY_DOWN:
      if (selected + 1 < static_cast<int>(records.size())) {

        ++selected;
      }
      break;
    case '\n':
    case KEY_ENTER:
      if (!records.empty()) {
        const auto &command =
            records[static_cast<std::size_t>(selected)].command;

        if (copyToClipboard(command)) {
          mvaddstr(LINES - 1, 2, "Command copied to clipboard!");
        } else {
          mvaddstr(LINES - 1, 2, "Clipboard copy failed.");
        }

        refresh();
      }
      break;

    case '/':
      query = prompt("Search command history/ask about a command: ");
      try {
        records = searchWithLoading(database, query);

        if (records.empty() && !query.empty()) {

          records = database.searchCommands(query);
        }
        selected = 0;
      } catch (const std::exception &error) {
        // if ollama ain't working

        try {

          records = database.searchCommands(query);
          selected = 0;

          if (records.empty()) {
            mvprintw(LINES - 1, 2, "SEARCH FAILED: %.100s", error.what());

            refresh();
            getch();
          }
        } catch (...) {
          mvaddstr(LINES - 1, 2, "Semantic and literal search failed.");
          refresh();
          getch();
        }
      }
      break;
    }
  }
}

void runTui(univecho::Database &database) {
  if (!initscr()) {
    throw std::runtime_error("Couldn't initalize ncurses");
  }

  try {
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    std::string mode = "All memories";
    auto memories = database.list();
    int selected = 0;
    bool running = true;

    while (running) {
      selected = std::clamp(selected, 0,
                            std::max(0, static_cast<int>(memories.size()) - 1));

      draw(memories, selected, mode);
      const int key = getch();

      switch (key) {
      case 'p':
      case 'q':
        running = false;
        break;
      case 'c':
        runCommandHistory(database);
      case KEY_UP:
        if (selected > 0)
          --selected;
        break;

      case KEY_DOWN:
        if (selected + 1 < static_cast<int>(memories.size())) {
          ++selected;
        }
        break;

      case '\n':
      case KEY_ENTER:
        if (!memories.empty()) {
          showMemory(memories[static_cast<std::size_t>(selected)]);
        }
        break;

      case 'n': {
        const auto title = prompt("new memory title");
        if (title.empty())
          break;

        const auto content = prompt("Memory content");
        database.add(title, content);

        mode = "All memories";
        memories = database.list();
        selected = 0;
        break;
      }

      case '/': {
        const auto query = prompt("Search memories");
        try {
          memories = database.search(query);
          mode = "Search: " + query;
          selected = 0;
        } catch (const std::exception &e) {
          erase();
          mvprintw(2, 2, "Search failed: %s", e.what());
          mvaddstr(4, 2, "Press any key to continue");
          refresh();
          getch();
        }

        break;
      }

      case 'r':
        memories = database.list();

        mode = "All memories";
        selected = 0;
        break;
      }
    }

  } catch (...) {

    endwin();
    throw;
  }

  endwin();
}
void printHelp() {
  std::cout << "UNIVECHO - Universal Memory\n\n"
               "Usage:\n"
               "  univecho                            Open the TUI\n"
               "  univecho tui                        Open the TUI\n"
               "  univecho commands QUERY             Search command history\n"
               "  univecho add TITLE TEXT             Save a memory\n"
               "  univecho list                       List memories\n"
               "  univecho search QUERY               Search memories\n"
               "  univecho help                       Show this help\n";
}
} // namespace
//

int main(int argc, char *argv[]) {
  try {
    univecho::Database database(databasePath());

    if (argc == 1 || (argc == 2 && std::string(argv[1]) == "tui")) {
      runTui(database);
      return 0;
    }

    const std::string command = argv[1];

    if (command == "import-history") {
      const char *historyEnv = std::getenv("HISTFILE");

      std::filesystem::path historyPath;

      if (historyEnv && *historyEnv) {
        historyPath = historyEnv;
      } else {
        const char *home = std::getenv("HOME");

        if (!home || !*home) {
          std::cerr << "UNIVECHO: home is not set \n";
          return 1;
        }

        historyPath = std::filesystem::path(home) / ".config/zsh/.zsh_history";
      }

      try {
        auto entries = univecho::CommandHistoryImporter::parseZsh(historyPath);

        database.importCommands(entries);

        cout << "Imported: " << entries.size()
             << " parsed history entries from Zsh." << endl;
      } catch (const std::exception &error) {
        std::cerr << "UNIVECHO: " << error.what() << endl;
        return 1;
      }

      return 0;
    }

    if (command == "commands") {

      if (argc < 3) {
        std::cerr << "Usage: univecho commands QUERY\n";
        return 2;
      }

      const std::string query = joinArgs(argc, argv, 2);
      const auto results = database.searchCommands(query);

      if (results.empty()) {
        cout << "No matching commands found..." << endl;
        return 0;
      }

      for (const auto &record : results) {
        std::cout << "[" << record.sourceShell << "] ";

        if (record.timestamp) {
          const std::time_t timestamp = *record.timestamp;
          std::tm localTime{};

          if (localtime_r(&timestamp, &localTime)) {
            cout << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
          } else {
          }
        } else {
        }

        cout << endl << record.command << endl << endl;
      }

      cout << results.size() << " unique command(s).\n";
      return 0;
    }

    if (command == "help" || command == "--help" || command == "-h") {
      printHelp();
    } else if (command == "add") {
      if (argc < 4) {
        std::cerr << "Usage: univecho add TITLE TEXT\n";
        return 2;
      }

      database.add(argv[2], joinArgs(argc, argv, 3));
      std::cout << "Memory saved.\n";
    } else if (command == "list") {
      for (const auto &memory : database.list()) {
        std::cout << "#" << memory.id << "  " << memory.title << "\n"
                  << "  " << memory.content << "\n"
                  << "  " << memory.createdAt << "\n\n";
      }
    } else if (command == "search") {
      if (argc < 3) {
        std::cerr << "Usage: univecho search QUERY\n";
        return 2;
      }

      const auto results = database.search(joinArgs(argc, argv, 2));

      for (const auto &memory : results) {
        std::cout << "#" << memory.id << "  " << memory.title << "\n"
                  << "  " << memory.content << "\n\n";
      }

      std::cout << results.size() << " result(s).\n";
    }

    else {
      printHelp();
      return 2;
    }

    return 0;
  }

  catch (const std::exception &error) {
    std::cerr << "UNIVECHO ERROR: " << error.what() << "\n";
    return 1;
  }
}
