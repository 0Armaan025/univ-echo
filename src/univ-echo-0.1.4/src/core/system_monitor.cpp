#include "../../include/univ-echo/system_monitor.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace univecho {

double SystemMonitor::calculatedUsage(const CPUCounters &previous,
                                      const CPUCounters &current) {
  const auto totalDelta = current.total - previous.total;
  const auto idleDelta = current.idle - previous.idle;

  if (totalDelta == 0)
    return 0.0;

  const double usage = 100.0 * static_cast<double>(totalDelta - idleDelta) /
                       static_cast<double>(totalDelta);

  return std::clamp(usage, 0.0, 100.0);
}

SystemMetrics SystemMonitor::sample() {
  SystemMetrics metrics;

#ifndef __linux__
  throw std::runtime_error("System Monitor currently supports Linux only.");
#else

  // CPU: /proc/stat

  std::ifstream statFile("/proc/stat");

  if (!statFile)
    throw std::runtime_error("Cannot read /proc/stat");

  std::vector<CPUCounters> currentCores;
  CPUCounters currentTotal;

  std::string line;

  while (std::getline(statFile, line)) {
    std::istringstream stream(line);

    std::string name;

    if (!(stream >> name))
      continue;

    if (name != "cpu" && !(name.size() > 3 && name.substr(0, 3) == "cpu"))
      break;

    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t system = 0;
    std::uint64_t idle = 0;
    std::uint64_t iowait = 0;
    std::uint64_t irq = 0;
    std::uint64_t softirq = 0;
    std::uint64_t steal = 0;

    stream >> user >> nice >> system >> idle >> iowait >> irq >> softirq >>
        steal;

    CPUCounters counters;

    counters.total =
        user + nice + system + idle + iowait + irq + softirq + steal;

    counters.idle = idle + iowait;

    if (name == "cpu") {
      currentTotal = counters;
    } else {
      currentCores.push_back(counters);

      metrics.cores.push_back({name, 0.0});
    }
  }

  if (hasPreviousCPU_) {
    metrics.cpuUsage = calculatedUsage(previousTotalCPU_, currentTotal);

    const std::size_t count =
        std::min(currentCores.size(), previousCores_.size());

    for (std::size_t i = 0; i < count; ++i) {
      metrics.cores[i].usagePercent =
          calculatedUsage(previousCores_[i], currentCores[i]);
    }
  }

  previousTotalCPU_ = currentTotal;
  previousCores_ = currentCores;
  hasPreviousCPU_ = true;

  // Memory: /proc/meminfo

  std::ifstream memoryFile("/proc/meminfo");

  if (!memoryFile)
    throw std::runtime_error("Cannot read /proc/meminfo");

  std::uint64_t totalRAM = 0;
  std::uint64_t availableRAM = 0;
  std::uint64_t freeRAM = 0;
  std::uint64_t buffers = 0;
  std::uint64_t cached = 0;

  std::uint64_t totalSwap = 0;
  std::uint64_t freeSwap = 0;

  std::string key;
  std::uint64_t value = 0;
  std::string unit;

  while (memoryFile >> key >> value >> unit) {
    if (key == "MemTotal:")
      totalRAM = value;
    else if (key == "MemAvailable:")
      availableRAM = value;
    else if (key == "MemFree:")
      freeRAM = value;
    else if (key == "Buffers:")
      buffers = value;
    else if (key == "Cached:")
      cached = value;
    else if (key == "SwapTotal:")
      totalSwap = value;
    else if (key == "SwapFree:")
      freeSwap = value;
  }

  // Fallback for kernels without MemAvailable.

  if (availableRAM == 0)
    availableRAM = freeRAM + buffers + cached;

  metrics.totalRAM = totalRAM * 1024;
  metrics.availableRAM = availableRAM * 1024;

  metrics.usedRAM = metrics.totalRAM > metrics.availableRAM
                        ? metrics.totalRAM - metrics.availableRAM
                        : 0;

  metrics.totalSwap = totalSwap * 1024;
  metrics.freeSwap = freeSwap * 1024;

  // Uptime: /proc/uptime

  std::ifstream uptimeFile("/proc/uptime");

  if (uptimeFile)
    uptimeFile >> metrics.uptimeSeconds;

  // Load average: /proc/loadavg

  std::ifstream loadFile("/proc/loadavg");

  if (loadFile) {
    loadFile >> metrics.load1 >> metrics.load5 >> metrics.load15;
  }

  return metrics;

#endif
}
} // namespace univecho
