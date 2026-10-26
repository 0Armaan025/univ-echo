#include "../include/univ-echo/database.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cstdlib>
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
}

} // namespace
