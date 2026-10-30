#include "../../include/univ-echo/resource_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {
bool readMemoryValue(const char *path, std::uint64_t &value) {
  std::ifstream file(path);
  std::string text;

  if (!(file >> text) || text == "max") {
    return false;
  }

  try {
    std::size_t used = 0;
    const auto parsed = std::stoull(text, &used);

    if (used != text.size()) {
      return false;
    }

    value = parsed;

    return true;
  } catch (...) {
    return false;
  }
}

bool readMemInfo(const char *key, std::uint64_t &bytes) {

  std::ifstream file("/proc/meminfo");
  std::string name;
  std::uint64_t amount = 0;
  std::string unit;

  while (file >> name >> amount >> unit) {
    if (name == key) {
      // linux in kib

      bytes = amount * 1024ULL;
      return true;
    }
  }

  return false;
}

} // namespace
namespace univecho {
ResourceSnapshot detectResources() {
  ResourceSnapshot result;

  const unsigned int detectedThreads = std::thread::hardware_concurrency();

  result.logicalCpuThreads = detectedThreads > 0 ? detectedThreads : 1;

#ifdef __linux__

  std::uint64_t hostTotal = 0;
  std::uint64_t hostAvailable = 0;

  bool totalKnown = readMemInfo("MemTotal:", hostTotal);
  bool availableKnown = readMemInfo("MemAvailable:", hostAvailable);

  // if not available
  if (!totalKnown || !availableKnown) {
    const long pageSize = sysconf(_SC_PAGESIZE);

    const long totalPages = sysconf(_SC_PHYS_PAGES);
    const long availablePages = sysconf(_SC_AVPHYS_PAGES);
    if (pageSize > 0 && totalPages > 0) {
      hostTotal = static_cast<std::uint64_t>(pageSize) *
                  static_cast<std::uint64_t>(totalPages);

      totalKnown = true;
    }

    if (pageSize > 0 && availablePages >= 0) {
      hostAvailable = static_cast<std::uint64_t>(pageSize) *
                      static_cast<std::uint64_t>(availablePages);

      availableKnown = true;
    }
  }

  if (totalKnown) {
    result.totalRamBytes = hostTotal;
  }

  if (availableKnown) {
    result.availableRamBytes = hostAvailable;
  }

  // cgroup v2 memories

  std::uint64_t limit = 0;
  std::uint64_t current = 0;

  bool hasLimit = readMemoryValue("/sys/fs/cgroup/memory.max", limit);
  bool hasCurrent = readMemoryValue("/sys/fs/cgroup/memory.current", current);

  if (!hasLimit || !hasCurrent) {
    hasLimit =
        readMemoryValue("/sys/fs/cgroup/memory/memory.limit_in_bytes", limit);
    hasCurrent =
        readMemoryValue("/sys/fs/cgroup/memory/memory.usage_in_bytes", current);
  }

  // ignore unlimited

  constexpr std::uint64_t GiB = 1024ULL * 1024 * 1024;
  const bool plausibleLimit = hasLimit && limit < (1ULL << 60);

  if (plausibleLimit && hasCurrent) {
    const std::uint64_t cgroupAvailable = current < limit ? limit - current : 0;

    if (result.totalRamBytes == 0 || limit < result.totalRamBytes) {
      result.totalRamBytes = limit;
    }

    if (result.availableRamBytes == 0 ||
        cgroupAvailable < result.availableRamBytes) {
      result.availableRamBytes = cgroupAvailable;
    }
  }

#endif

#ifdef _WIN32
  MEMORYSTATUSEX memoryStatus{};
  memoryStatus.dwLength = sizeof(memoryStatus);

  if (GlobalMemoryStatusEx(&memoryStatus)) {
    result.totalRamBytes = memoryStatus.ullTotalPhys;
    result.availableRamBytes = memoryStatus.ullAvailPhys;
  }
#endif

  if (result.totalRamBytes > 0) {
    result.availableRamBytes =
        std::min(result.availableRamBytes, result.totalRamBytes);
  }

  const bool lowAbsoluteMemory = result.availableRamBytes < GiB;

  const bool lowRelativeMemory =
      result.totalRamBytes > 0 &&
      result.availableRamBytes < result.totalRamBytes / 10;

  result.memoryPressure = lowAbsoluteMemory || lowRelativeMemory;
  return result;
}

ResourceProfile chooseResourceProfile(const ResourceSnapshot &resources) {
  ResourceProfile profile;

  // leave one logical cpu

  profile.cpuThreads =
      resources.logicalCpuThreads > 1 ? resources.logicalCpuThreads - 1 : 1;

  if (resources.totalRamBytes == 0 || resources.availableRamBytes == 0) {
    return profile;
  }

  const std::uint64_t available = resources.availableRamBytes;
  constexpr std::uint64_t GiB = 1024ULL * 1024 * 1024;

  if (available < GiB) {
    profile.embeddingBatchSize = 4;
    profile.candidateLimit = 75;
  } else if (available > 2 * GiB) {
    profile.embeddingBatchSize = 8;
    profile.candidateLimit = 100;
  } else if (available < 4 * GiB) {
    profile.embeddingBatchSize = 16;
    profile.candidateLimit = 200;
  } else {
    profile.embeddingBatchSize = 32;
    profile.candidateLimit = 400;
  }

  // reduceee the workload yaey

  if (resources.memoryPressure) {
    profile.embeddingBatchSize =
        std::min<std::size_t>(profile.embeddingBatchSize, 4);

    profile.candidateLimit = std::min<std::size_t>(profile.candidateLimit, 50);
    // alright this is a lil intelluctally tiring thing but W, we are gonna do
    // it yaey
  }

  return profile;
}
} // namespace univecho
