#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace univecho {
struct CPUCore {
  std::string name;
  double usagePercent{};
};

struct SystemMetrics {
  double cpuUsage{};
  std::vector<CPUCore> cores;

  std::uint64_t totalRAM{};
  std::uint64_t availableRAM{};
  std::uint64_t usedRAM{};

  std::uint64_t totalSwap{};
  std::uint64_t freeSwap{};

  double uptimeSeconds{};

  double load1{};
  double load5{};
  double load15{};
};

class SystemMonitor {
public:
  SystemMetrics sample();

private:
  struct CPUCounters {
    std::uint64_t total{};
    std::uint64_t idle{};
  };

  CPUCounters previousTotalCPU_{};
  std::vector<CPUCounters> previousCores_;
  bool hasPreviousCPU_{false};

  static double calculatedUsage(const CPUCounters &previous,
                                const CPUCounters &current);
};

} // namespace univecho
