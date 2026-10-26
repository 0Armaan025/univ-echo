#include "../../include/univ-echo/command_history.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace univecho {
namespace {
bool parseUnsigned(const std::string &text, unsigned long long &value) {
  if (text.empty()) {
    return false;
  }

  value = 0;

  for (const char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }

    const unsigned digit = static_cast<unsigned>(ch - '0');

    if (value > (std::numeric_limits<unsigned long long>::max() - digit) / 10) {
      return false;
    }

    value = value * 10 + digit;
  }

  return true;
}
} // namespace

std::vector<CommandHistoryEntry>
CommandHistoryImporter::parseZsh(const std::filesystem::path &path) {
  std::ifstream file(path);

  if (!file) {
    throw std::runtime_error("Cannot open zsh history file: " + path.string());
  }

  std::vector<CommandHistoryEntry> entries;
  std::string line;

  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    CommandHistoryEntry entry;
    entry.sourceShell = "zsh";

    if (line.rfind(": ", 0) == 0) {
      const std::size_t firstColon = line.find(":", 2);
      const std::size_t semiColon = line.find(';');

      if (firstColon != std::string::npos && semiColon != std::string::npos &&
          firstColon < semiColon) {
        const std::string timestampText = line.substr(2, firstColon - 2);

        const std::string durationText =
            line.substr(firstColon + 1, semiColon - firstColon - 1);

        unsigned long long timestampValue = 0;
        unsigned long long durationValue = 0;

        const bool validTimestamp =
            parseUnsigned(timestampText, timestampValue);

        const bool validDuration = parseUnsigned(durationText, durationValue);

        if (validTimestamp &&
            timestampValue <= static_cast<unsigned long long>(
                                  std::numeric_limits<std::time_t>::max()) &&
            validDuration &&
            durationValue <= std::numeric_limits<unsigned int>::max()) {

          entry.timestamp = static_cast<std::time_t>(timestampValue);
          entry.duration = static_cast<unsigned int>(durationValue);

          entry.command = line.substr(semiColon + 1);
        }

        else {
          entry.command = line;
        }
      } else {
        entry.command = line;
      }

    }

    else {
      entry.command = line;
    }

    if (!entry.command.empty()) {
      entries.push_back(std::move(entry));
    }
  }

  if (file.bad()) {
    throw std::runtime_error("Error reading zsh history file");
  }

  return entries;
}
} // namespace univecho
