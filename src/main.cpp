#include "../include/univ-echo/command_history.hpp"
#include "../include/univ-echo/database.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

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

  mvaddstr(LINES - 3, 2, "[n] New [/] Search [r] All [Enter] Open");
  mvaddstr(LINES - 2, 2, "[Up/Down] Navigate    [q] Quit");

  refresh();
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
  std::cout << "ECHO - Universal Memory\n\n"
               "Usage:\n"
               "  univecho                 Open the TUI\n"
               "  univecho tui             Open the TUI\n"
               "  univecho add TITLE TEXT  Save a memory\n"
               "  univecho list            List memories\n"
               "  univecho search QUERY    Search memories\n"
               "  univecho help            Show this help\n";
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
