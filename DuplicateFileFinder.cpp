#include "DuplicateFileFinder.hpp"

#include <QCryptographicHash>
#include <QFile>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr qint64 bufferSize{ 64 * 1024 };
    constexpr qint64 sampleSize{ 64 * 1024 };

    [[nodiscard]]
    QString pathToQString(
        const std::filesystem::path& path
    )
    {
        #ifdef _WIN32
        return QString::fromStdWString(path.wstring());
        #else
        return QString::fromStdString(path.string());
        #endif
    }

    [[nodiscard]]
    std::optional<QByteArray> quickFingerprint(
        const std::filesystem::path& path,
        const std::atomic_bool& cancelRequested
    )
    {
        QFile file{ pathToQString(path) };

        if (!file.open(QIODevice::ReadOnly))
        {
            return std::nullopt;
        }

        if (cancelRequested.load(std::memory_order_relaxed))
        {
            return std::nullopt;
        }

        QCryptographicHash hasher{
            QCryptographicHash::Sha256
        };

        const auto fileSize = file.size();

        if (fileSize <= sampleSize * 2)
        {
            const auto data = file.readAll();

            if (file.error() != QFileDevice::NoError)
            {
                return std::nullopt;
            }

            hasher.addData(data);
            return hasher.result();
        }

        const auto firstChunk = file.read(sampleSize);

        if (firstChunk.size() != sampleSize)
        {
            return std::nullopt;
        }

        hasher.addData(firstChunk);

        if (cancelRequested.load(std::memory_order_relaxed))
        {
            return std::nullopt;
        }

        if (!file.seek(fileSize - sampleSize))
        {
            return std::nullopt;
        }

        const auto lastChunk = file.read(sampleSize);

        if (lastChunk.size() != sampleSize)
        {
            return std::nullopt;
        }

        hasher.addData(lastChunk);

        return hasher.result();
    }

    // Hash one file without loading the entire file into memory.
    [[nodiscard]]
    std::optional<QByteArray> hashFile(
        const std::filesystem::path& path,
        const std::atomic_bool& cancelRequested
    )
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
            // Allow a running scan to stop quickly.
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

DuplicateSearchResult DuplicateFileFinder::find(
    const DuplicateSearchOptions& options,
    const std::atomic_bool& cancelRequested
)
{
    if (options.folderPath.trimmed().isEmpty())
    {
        return {
            {},
            QStringLiteral("Please select a folder."),
            false
        };
    }

       #ifdef _WIN32
    const std::filesystem::path root{
        options.folderPath.toStdWString()
    };
    #else
    const std::filesystem::path root{
        options.folderPath.toStdString()
    };
    #endif

    if (!std::filesystem::exists(root) ||
        !std::filesystem::is_directory(root))
    {
        return {
            {},
            QStringLiteral(
                "The selected folder does not exist."
            ),
            false
        };
    }

    // First group files by size.
    std::unordered_map<
        std::uintmax_t,
        std::vector<std::filesystem::path>
    > filesBySize;

    try
    {
        const auto processFile =
            [&filesBySize](
                const std::filesystem::directory_entry& entry)
            {
                std::error_code error;

                // Some Windows/system files cannot be inspected.
                // Skip them instead of failing the entire scan.
                if (!entry.is_regular_file(error) || error)
                {
                    return;
                }

                const auto size = entry.file_size(error);

                if (error)
                {
                    return;
                }

                filesBySize[size].push_back(
                    entry.path()
                );
            };

        if (options.recursive)
        {
            std::error_code error;

            auto iterator =
                std::filesystem::recursive_directory_iterator{
                    root,
                    std::filesystem::directory_options::
                        skip_permission_denied,
                    error
            };

            const auto end =
                std::filesystem::recursive_directory_iterator{};

            while (iterator != end)
            {
                // Stop if the user pressed Cancel.
                if (cancelRequested.load(
                    std::memory_order_relaxed))
                {
                    return {
                        {},
                        {},
                        true
                    };
                }

                // Only process the entry if accessing it succeeded.
                if (!error)
                {
                    processFile(*iterator);
                }

                error.clear();

                // Move to the next file/folder without throwing an exception.
                iterator.increment(error);

                // Ignore inaccessible Windows folders and continue where possible.
                if (error)
                {
                    error.clear();
                }
            }
        }
        else
        {
            for (const auto& entry :
                std::filesystem::directory_iterator{
                    root,
                    std::filesystem::directory_options::
                        skip_permission_denied
                })
            {
                if (cancelRequested.load(
                    std::memory_order_relaxed))
                {
                    return {
                        {},
                        {},
                        true
                    };
                }

                processFile(entry);
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return {
            {},
            QStringLiteral(
                "An error occurred while scanning the folder."
            ),
            false
        };
    }

    std::vector<DuplicateGroup> duplicateGroups;

    // when files the same size
    // Then use a quick sample before doing a full hash
    for (const auto& [size, files] : filesBySize)
    {
        if (files.size() < 2)
        {
            continue;
        }

        // quickly compare the start/end of same sized files
        std::unordered_map<
            std::string,
            std::vector<std::filesystem::path>
        > filesByQuickHash;

        for (const auto& file : files)
        {
            if (cancelRequested.load(std::memory_order_relaxed))
            {
                return {
                    std::move(duplicateGroups),
                    {},
                    true
                };
            }

            const auto quickHash =
                quickFingerprint(file, cancelRequested);

            if (!quickHash.has_value())
            {
                continue;
            }

            filesByQuickHash[
                quickHash->toHex().toStdString()
            ].push_back(file);
        }

        // only fully hash files whose quick fingerprints match.
        for (const auto& [_, candidates] : filesByQuickHash)
        {
            if (candidates.size() < 2)
            {
                continue;
            }

            std::unordered_map<
                std::string,
                std::vector<std::filesystem::path>
            > filesByFullHash;

            for (const auto& file : candidates)
            {
                if (cancelRequested.load(std::memory_order_relaxed))
                {
                    return {
                        std::move(duplicateGroups),
                        {},
                        true
                    };
                }

                const auto fullHash =
                    hashFile(file, cancelRequested);

                if (!fullHash.has_value())
                {
                    continue;
                }

                filesByFullHash[
                    fullHash->toHex().toStdString()
                ].push_back(file);
            }

            // matching full hashes are real duplicate groups
            for (const auto& [hash, matchingFiles] : filesByFullHash)
            {
                if (matchingFiles.size() < 2)
                {
                    continue;
                }

                DuplicateGroup group{
                    .sizeBytes = size,
                    .hash = QString::fromStdString(hash)
                };

                group.filePaths.reserve(matchingFiles.size());

                for (const auto& file : matchingFiles)
                {
                    group.filePaths.push_back(
                        pathToQString(file)
                    );
                }

                duplicateGroups.push_back(
                    std::move(group)
                );
            }
        }
    }

    return {
        std::move(duplicateGroups),
        {},
        false
    };
}