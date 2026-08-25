#include "FolderSynchroniser.hpp"

#include <QCryptographicHash>
#include <QFile>

#include <filesystem>
#include <optional>
#include <system_error>

namespace
{
    constexpr qint64 bufferSize{ 64 * 1024 };

    [[nodiscard]]
    QString pathToQString(
        const std::filesystem::path& path)
    {
#ifdef _WIN32
        return QString::fromStdWString(path.wstring());
#else
        return QString::fromStdString(path.string());
#endif
    }

    // Hash a file in chunks so large files are not loaded
    // entirely into memory.
    [[nodiscard]]
    std::optional<QByteArray> hashFile(
        const std::filesystem::path& path,
        const std::atomic_bool& cancelRequested)
    {
        QFile file{ pathToQString(path) };

        if (!file.open(QIODevice::ReadOnly))
        {
            return std::nullopt;
        }

        QCryptographicHash hasher{
            QCryptographicHash::Sha256
        };

        while (!file.atEnd())
        {
            if (cancelRequested.load(
                std::memory_order_relaxed))
            {
                return std::nullopt;
            }

            const auto data = file.read(bufferSize);

            if (data.isEmpty() &&
                file.error() != QFileDevice::NoError)
            {
                return std::nullopt;
            }

            hasher.addData(data);
        }

        return hasher.result();
    }
}

FolderSyncResult FolderSynchroniser::compare(
    const FolderSyncOptions& options,
    const std::atomic_bool& cancelRequested)
{
    if (options.sourceFolder.trimmed().isEmpty())
    {
        return {
            {},
            QStringLiteral("Please select a source folder."),
            false
        };
    }

    if (options.destinationFolder.trimmed().isEmpty())
    {
        return {
            {},
            QStringLiteral("Please select a destination folder."),
            false
        };
    }

#ifdef _WIN32
    const std::filesystem::path sourceRoot{
        options.sourceFolder.toStdWString()
    };

    const std::filesystem::path destinationRoot{
        options.destinationFolder.toStdWString()
    };
#else
    const std::filesystem::path sourceRoot{
        options.sourceFolder.toStdString()
    };

    const std::filesystem::path destinationRoot{
        options.destinationFolder.toStdString()
    };
#endif

    std::error_code error;

    if (!std::filesystem::is_directory(
        sourceRoot,
        error))
    {
        return {
            {},
            QStringLiteral(
                "The source folder does not exist."
            ),
            false
        };
    }

    error.clear();

    if (!std::filesystem::is_directory(
        destinationRoot,
        error))
    {
        return {
            {},
            QStringLiteral(
                "The destination folder does not exist."
            ),
            false
        };
    }

    // this prevents accidentally comparing a folder with itself. i hope*
    error.clear();

    if (std::filesystem::equivalent(
        sourceRoot,
        destinationRoot,
        error) &&
        !error)
    {
        return {
            {},
            QStringLiteral(
                "Source and destination cannot be the same folder."
            ),
            false
        };
    }

    std::vector<FolderSyncItem> items;

    const auto processFile =
        [&](const std::filesystem::directory_entry& entry)
        -> std::optional<QString>
        {
            std::error_code fileError;

            if (!entry.is_regular_file(fileError) ||
                fileError)
            {
                return std::nullopt;
            }

            const auto sourcePath = entry.path();

            // Example:
            // C:\Source\assets\icon.png
            // becomes:
            // assets\icon.png
            const auto relativePath =
                sourcePath.lexically_relative(sourceRoot);

            const auto destinationPath =
                destinationRoot / relativePath;

            const auto sourceSize =
                entry.file_size(fileError);

            if (fileError)
            {
                return QStringLiteral(
                    "Unable to read file information for: %1"
                ).arg(pathToQString(sourcePath));
            }

            // File does not exist in Destination
            if (!std::filesystem::exists(
                destinationPath,
                fileError))
            {
                items.push_back({
                    .relativePath =
                        pathToQString(relativePath),
                    .sizeBytes = sourceSize,
                    .status = SyncStatus::New,
                    .action = SyncAction::Copy
                    });

                return std::nullopt;
            }

            if (fileError)
            {
                return QStringLiteral(
                    "Unable to inspect destination file: %1"
                ).arg(pathToQString(destinationPath));
            }

            // If the destination path isn't a normal file, it cannot be considered identical

            if (!std::filesystem::is_regular_file(
                destinationPath,
                fileError) ||
                fileError)
            {
                items.push_back({
                    .relativePath =
                        pathToQString(relativePath),
                    .sizeBytes = sourceSize,
                    .status = SyncStatus::Changed,
                    .action = SyncAction::Update
                    });

                return std::nullopt;
            }

            const auto destinationSize =
                std::filesystem::file_size(
                    destinationPath,
                    fileError);

            if (fileError)
            {
                return QStringLiteral(
                    "Unable to read destination file information: %1"
                ).arg(pathToQString(destinationPath));
            }

            // Different sizes immediately mean different contents.
            if (sourceSize != destinationSize)
            {
                items.push_back({
                    .relativePath =
                        pathToQString(relativePath),
                    .sizeBytes = sourceSize,
                    .status = SyncStatus::Changed,
                    .action = SyncAction::Update
                    });

                return std::nullopt;
            }

            // Same size, so compare the actual file contents.
            const auto sourceHash =
                hashFile(
                    sourcePath,
                    cancelRequested
                );

            if (cancelRequested.load(
                std::memory_order_relaxed))
            {
                return std::nullopt;
            }

            const auto destinationHash =
                hashFile(
                    destinationPath,
                    cancelRequested
                );

            if (cancelRequested.load(
                std::memory_order_relaxed))
            {
                return std::nullopt;
            }

            if (!sourceHash.has_value() ||
                !destinationHash.has_value())
            {
                return QStringLiteral(
                    "Unable to compare file contents: %1"
                ).arg(pathToQString(relativePath));
            }

            const bool identical =
                *sourceHash == *destinationHash;

            items.push_back({
                .relativePath =
                    pathToQString(relativePath),
                .sizeBytes = sourceSize,
                .status = identical
                    ? SyncStatus::Unchanged
                    : SyncStatus::Changed,
                .action = identical
                    ? SyncAction::None
                    : SyncAction::Update
                });

            return std::nullopt;
        };

    // walk through the source folder and compare each file
    try
    {
        if (options.recursive)
        {
            std::error_code iteratorError;

            auto iterator =
                std::filesystem::recursive_directory_iterator{
                    sourceRoot,
                    std::filesystem::directory_options::
                        skip_permission_denied,
                    iteratorError
            };

            const auto end =
                std::filesystem::recursive_directory_iterator{};

            while (iterator != end)
            {
                // stop if the i press cancel
                if (cancelRequested.load(
                    std::memory_order_relaxed))
                {
                    return {
                        std::move(items),
                        {},
                        true
                    };
                }

                if (!iteratorError)
                {
                    if (const auto fileError =
                        processFile(*iterator);
                        fileError.has_value())
                    {
                        return {
                            {},
                            *fileError,
                            false
                        };
                    }
                }

                iteratorError.clear();
                iterator.increment(iteratorError);

                // Ignore folders Windows will not let me access.
                if (iteratorError)
                {
                    iteratorError.clear();
                }
            }
        }
        else
        {
            for (const auto& entry :
                std::filesystem::directory_iterator{
                    sourceRoot,
                    std::filesystem::directory_options::
                        skip_permission_denied
                })
            {
                if (cancelRequested.load(
                    std::memory_order_relaxed))
                {
                    return {
                        std::move(items),
                        {},
                        true
                    };
                }

                if (const auto fileError =
                    processFile(entry);
                    fileError.has_value())
                {
                    return {
                        {},
                        *fileError,
                        false
                    };
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return {
            {},
            QStringLiteral(
                "An error occurred while comparing the folders."
            ),
            false
        };
    }

    // Comparison completed successfully
    return {
        std::move(items),
        {},
        false
    };
}
        QString FolderSynchroniser::statusToString(
            const SyncStatus status)
        {
            switch (status)
            {
            case SyncStatus::New:
                return QStringLiteral("New");

            case SyncStatus::Changed:
                return QStringLiteral("Changed");

            case SyncStatus::Unchanged:
                return QStringLiteral("Unchanged");
            }

            return QStringLiteral("Unknown");
        }

        QString FolderSynchroniser::actionToString(
            const SyncAction action)
        {
            switch (action)
            {
            case SyncAction::Copy:
                return QStringLiteral("Copy");

            case SyncAction::Update:
                return QStringLiteral("Update");

            case SyncAction::None:
                return QStringLiteral("None");
            }

            return QStringLiteral("None");
        }

        FolderSynchroniseResult FolderSynchroniser::synchronise(
            const FolderSyncOptions& options,
            const std::vector<FolderSyncItem>& items,
            const std::atomic_bool& cancelRequested)
        {
            #ifdef _WIN32
            const std::filesystem::path sourceRoot{
                options.sourceFolder.toStdWString()
            };

            const std::filesystem::path destinationRoot{
                options.destinationFolder.toStdWString()
            };
            #else
            const std::filesystem::path sourceRoot{
                options.sourceFolder.toStdString()
            };

            const std::filesystem::path destinationRoot{
                options.destinationFolder.toStdString()
            };
            #endif

            FolderSynchroniseResult result{};

            for (const auto& item : items)
            {
                // Stop safely if Cancel was requested.
                if (cancelRequested.load(
                    std::memory_order_relaxed))
                {
                    result.cancelled = true;
                    return result;
                }

                // Unchanged files require no work.
                if (item.action == SyncAction::None)
                {
                    continue;
                }

                #ifdef _WIN32
                const std::filesystem::path relativePath{
                    item.relativePath.toStdWString()
                };
                #else
                const std::filesystem::path relativePath{
                    item.relativePath.toStdString()
                };
                #endif

                const auto sourcePath =
                    sourceRoot / relativePath;

                const auto destinationPath =
                    destinationRoot / relativePath;

                std::error_code error;

                // Make any missing destination subfolders.
                const auto parentFolder =
                    destinationPath.parent_path();

                if (!parentFolder.empty())
                {
                    std::filesystem::create_directories(
                        parentFolder,
                        error
                    );

                    if (error)
                    {
                        result.error =
                            QStringLiteral(
                                "Unable to create destination folder for: %1"
                            ).arg(item.relativePath);

                        return result;
                    }
                }

                error.clear();

                switch (item.action)
                {
                case SyncAction::Copy:
                {
                    // New files should not unexpectedly overwrite
                    // something that appeared after comparison.
                    const bool copied =
                        std::filesystem::copy_file(
                            sourcePath,
                            destinationPath,
                            std::filesystem::copy_options::none,
                            error
                        );

                    if (!copied || error)
                    {
                        result.error =
                            QStringLiteral(
                                "Unable to copy file: %1"
                            ).arg(item.relativePath);

                        return result;
                    }

                    ++result.copiedFiles;
                    break;
                }

                case SyncAction::Update:
                {
                    const bool updated =
                        std::filesystem::copy_file(
                            sourcePath,
                            destinationPath,
                            std::filesystem::copy_options::
                            overwrite_existing,
                            error
                        );

                    if (!updated || error)
                    {
                        result.error =
                            QStringLiteral(
                                "Unable to update file: %1"
                            ).arg(item.relativePath);

                        return result;
                    }

                    ++result.updatedFiles;
                    break;
                }

                case SyncAction::None:
                    break;
                }
            }

            return result;
        }