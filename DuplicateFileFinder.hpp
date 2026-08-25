#pragma once

#include <QString>

#include <atomic>
#include <cstdint>
#include <vector>

// This is where i choose the Settings
struct DuplicateSearchOptions final
{
    QString folderPath;
    bool recursive{ true };
};


struct DuplicateGroup final
{
    std::vector<QString> filePaths;
    std::uintmax_t sizeBytes{};
    QString hash;
};

//this will be the Overall result of the duplicate scan
struct DuplicateSearchResult final
{
    std::vector<DuplicateGroup> groups;
    QString error;
    bool cancelled{ false };

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class DuplicateFileFinder final
{
public:
    [[nodiscard]]
    static DuplicateSearchResult find(
        const DuplicateSearchOptions& options,
        const std::atomic_bool& cancelRequested
    );
};
