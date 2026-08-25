#include "SystemMonitor.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <string>
#include <intrin.h>

namespace
{
    // converts windows filetime into a normal 64-bit number
    [[nodiscard]]
    std::uint64_t fileTimeToUint64(
        const FILETIME& fileTime) noexcept
    {
        const auto high =
            static_cast<std::uint64_t>(
                fileTime.dwHighDateTime
                );

        const auto low =
            static_cast<std::uint64_t>(
                fileTime.dwLowDateTime
                );

        return (high << 32) | low;
    }
}

QString SystemMonitor::getCpuName()
{
    std::array<int, 4> cpuInfo{};
    std::array<char, 49> brand{};

    // CPU brand text is stored across these three CPUID calls.
    for (int i = 0; i < 3; ++i)
    {
        __cpuid(
            cpuInfo.data(),
            0x80000002 + i
        );

        std::memcpy(
            brand.data() + (i * 16),
            cpuInfo.data(),
            16
        );
    }

    return QString::fromLatin1(brand.data()).trimmed();
}

std::uint32_t SystemMonitor::getLogicalProcessorCount()
{
    SYSTEM_INFO systemInfo{};

    GetNativeSystemInfo(&systemInfo);

    return static_cast<std::uint32_t>(
        systemInfo.dwNumberOfProcessors
        );
}

std::vector<QString> SystemMonitor::getGpuNames()
{
    using Microsoft::WRL::ComPtr;

    std::vector<QString> gpuNames;

    ComPtr<IDXGIFactory1> factory;

    if (FAILED(CreateDXGIFactory1(
        IID_PPV_ARGS(&factory))))
    {
        return gpuNames;
    }

    for (UINT index = 0;; ++index)
    {
        ComPtr<IDXGIAdapter1> adapter;

        const auto result =
            factory->EnumAdapters1(
                index,
                &adapter
            );

        if (result == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        if (FAILED(result))
        {
            continue;
        }

        DXGI_ADAPTER_DESC1 description{};

        if (FAILED(adapter->GetDesc1(&description)))
        {
            continue;
        }

        // Ignore software-only graphics adapters.
        if ((description.Flags &
            DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }

        gpuNames.emplace_back(
            QString::fromWCharArray(
                description.Description
            ).trimmed()
        );
    }

    return gpuNames;
}

SystemStats SystemMonitor::sample()
{
    SystemStats stats{};

    // Hardware information doesn't change while the program is running,
    // so retrieve it once and reuse it.
    static const auto cpuName =
        getCpuName();

    static const auto gpuNames =
        getGpuNames();

    static const auto processorCount =
        getLogicalProcessorCount();

    stats.cpuName = cpuName;
    stats.gpuNames = gpuNames;
    stats.logicalProcessorCount = processorCount;

    stats.cpuUsagePercent = sampleCpuUsage();

    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);

    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        stats.totalMemoryBytes =
            memoryStatus.ullTotalPhys;

        stats.usedMemoryBytes =
            memoryStatus.ullTotalPhys -
            memoryStatus.ullAvailPhys;

        stats.memoryUsagePercent =
            static_cast<double>(
                memoryStatus.dwMemoryLoad
                );
    }

    stats.uptimeSeconds =
        static_cast<std::uint64_t>(
            GetTickCount64() / 1000
            );

    return stats;
    //^^^ will tell me approximately how long windows has been running in milliseconds.
}


std::optional<double> SystemMonitor::sampleCpuUsage()
{
    FILETIME idleTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};

    if (!GetSystemTimes(
        &idleTime,
        &kernelTime,
        &userTime))
    {
        return std::nullopt;
    }

    const auto idle =
        fileTimeToUint64(idleTime);

    const auto kernel =
        fileTimeToUint64(kernelTime);

    const auto user =
        fileTimeToUint64(userTime);

    // The first reading gives my starting point
    if (!hasPreviousCpuSample_)
    {
        previousIdleTime_ = idle;
        previousKernelTime_ = kernel;
        previousUserTime_ = user;

        hasPreviousCpuSample_ = true;

        return std::nullopt;
    }

    const auto idleDifference =
        idle - previousIdleTime_;

    const auto kernelDifference =
        kernel - previousKernelTime_;

    const auto userDifference =
        user - previousUserTime_;

    previousIdleTime_ = idle;
    previousKernelTime_ = kernel;
    previousUserTime_ = user;

    const auto totalTime =
        kernelDifference + userDifference;

    if (totalTime == 0)
    {
        return 0.0;
    }

    const auto busyTime =
        totalTime - idleDifference;

    const auto percentage =
        (static_cast<double>(busyTime) /
            static_cast<double>(totalTime)) *
        100.0;

    return std::clamp(
        percentage,
        0.0,
        100.0
    );
}