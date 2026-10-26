#pragma once
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

using namespace std;
namespace univecho {

struct CommandHistoryEntry {
  std::string command;

  std::optional<std::time_t> timestamp;

  std::optional<unsigned int> duration;

  std::string sourceShell;
};

class CommandHistoryImporter {
public:
  static std::vector<CommandHistoryEntry>
  parseZsh(const std::filesystem::path &path);
};

} // namespace univecho
