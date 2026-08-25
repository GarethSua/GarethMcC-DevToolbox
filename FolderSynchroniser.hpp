#pragma once

#include <QString>

#include <atomic>
#include <cstdint>
#include <vector>

enum class SyncStatus
{
    New,
    Changed,
    Unchanged
};

enum class SyncAction
{
    Copy,
    Update,
    None
};

// Repersents the options ill select
struct FolderSyncOptions final
{
    QString sourceFolder;
    QString destinationFolder;
    bool recursive{ true };
};

// Represents one file found during comparison
struct FolderSyncItem final
{
    QString relativePath;
    std::uintmax_t sizeBytes{};

    SyncStatus status{ SyncStatus::Unchanged };
    SyncAction action{ SyncAction::None };
};

// the overall result of comparing the two folders
struct FolderSyncResult final
{
    std::vector<FolderSyncItem> items;
    QString error;
    bool cancelled{ false };

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

struct FolderSynchroniseResult final
{
    std::size_t copiedFiles{};
    std::size_t updatedFiles{};

    QString error;
    bool cancelled{ false };

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class FolderSynchroniser final
{
public:
    [[nodiscard]]
    static FolderSyncResult compare(
        const FolderSyncOptions& options,
        const std::atomic_bool& cancelRequested
    );

    [[nodiscard]]
    static QString statusToString(
        SyncStatus status
    );

    [[nodiscard]]
    static QString actionToString(
        SyncAction action
    );

    [[nodiscard]]
    static FolderSynchroniseResult synchronise(
        const FolderSyncOptions& options,
        const std::vector<FolderSyncItem>& items,
        const std::atomic_bool& cancelRequested
    );
};
