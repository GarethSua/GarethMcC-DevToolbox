#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

struct SystemStats final
{
    QString cpuName;
    std::vector<QString> gpuNames;

    std::uint32_t logicalProcessorCount{};

    std::optional<double> cpuUsagePercent;

    std::uint64_t totalMemoryBytes{};
    std::uint64_t usedMemoryBytes{};
    double memoryUsagePercent{};

    std::uint64_t uptimeSeconds{};
};

class SystemMonitor final
{
public:
    [[nodiscard]]
    SystemStats sample();

private:
    [[nodiscard]]
    std::optional<double> sampleCpuUsage();

    [[nodiscard]]
    static QString getCpuName();

    [[nodiscard]]
    static std::vector<QString> getGpuNames();

    [[nodiscard]]
    static std::uint32_t getLogicalProcessorCount();

    std::uint64_t previousIdleTime_{};
    std::uint64_t previousKernelTime_{};
    std::uint64_t previousUserTime_{};

    bool hasPreviousCpuSample_{ false };
};

// this all stores CPU%, RAM used, Total RAM, RAM%, PC uptime 