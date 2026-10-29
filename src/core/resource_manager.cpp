#include "../../include/univ-echo/resource_manager.hpp"

#include <algorithm>
#include <thread>

#ifdef __linux__
#include <unistd.h>
#endif

namespace univecho {
ResourceSnapshot detectResources() {
  ResourceSnapshot result;

  const unsigned int detectedThreads = std::thread::hardware_concurrency();

  result.logicalCpuThreads = detectedThreads > 0 ? detectedThreads : 1;

#ifdef __linux__
  const long pageSize = sysconf(_SC_PAGESIZE);
  const long totalPages = sysconf(_SC_PHYS_PAGES);
  const long availablePages = sysconf(_SC_AVPHYS_PAGES);

  if (pageSize > 0 && totalPages > 0 && availablePages >= 0) {
    result.totalRamBytes = static_cast<std::uint64_t>(pageSize) *
                           static_cast<std::uint64_t>(totalPages);

    result.availableRamBytes = static_cast<std::uint64_t>(pageSize) *
                               static_cast<std::uint64_t>(availablePages);

    result.availableRamBytes =
        std::min(result.availableRamBytes, result.totalRamBytes);

    const bool lowAbsoluteMemory =
        result.availableRamBytes < 1024ULL * 1024 * 1024;

    const bool lowRelativeMemory =
        result.totalRamBytes > 0 &&
        result.availableRamBytes < result.totalRamBytes / 10;

    result.memoryPressure = lowAbsoluteMemory || lowRelativeMemory;
  }

#endif

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
    profile.candidateLimit = 50;
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
  }

  return profile;
}
} // namespace univecho
