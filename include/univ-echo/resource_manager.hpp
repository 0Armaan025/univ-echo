#include <cstdint>

namespace univecho {

struct ResourceSnapshot {

  unsigned int logicalCpuThreads = 1;
  std::uint64_t totalRamBytes = 0;
  std::uint64_t availableRamBytes = 0;
  bool memoryPressure = true;
};

struct ResourceProfile {
  unsigned int cpuThreads = 1;
  std::size_t embeddingBatchSize = 4;
  std::size_t candidateLimit = 50;
};

ResourceSnapshot detectResources();
ResourceProfile chooseResourceProfile(const ResourceSnapshot &resources);
} // namespace univecho
