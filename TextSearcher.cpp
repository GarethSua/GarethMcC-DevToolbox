#include "TextSearcher.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <filesystem>
#include <unordered_set>

namespace
{
    // Converts "*.cpp;*.hpp;*.txt" into individual extensions
    // that we can compare against each file we discover.

    [[nodiscard]]
    std::unordered_set<QString> parseExtensions(
        const QString& fileTypes
    )
    {
        std::unordered_set<QString> extensions;

        for (const auto& type :
            fileTypes.split(';', Qt::SkipEmptyParts))
        {
            auto extension = type.trimmed();

            // "*.cpp" becomes ".cpp"
            if (extension.startsWith('*'))
            {
                extension.removeFirst();
            }

            extensions.insert(extension.toLower());
        }

        return extensions;
    }

    // Im Checking whether this file has one of the extensions
    [[nodiscard]]
    bool shouldSearchFile(
        const std::filesystem::path& path,
        const std::unordered_set<QString>& extensions
    )
    {
        // No extension filter will mean search every file.
        if (extensions.empty())
        {
            return true;
        }

        const auto extension = QString::fromStdString(
            path.extension().string()
        ).toLower();

        return extensions.contains(extension);
    }

    // Will Open one file and check every line for the search text.
    void searchFile(
        const std::filesystem::path& path,
        const TextSearchOptions& options,
        std::vector<TextSearchMatch>& matches,
        const std::atomic_bool& cancelRequested
    )
    {
        QFile file{
            QString::fromStdString(path.string())
        };

        // If the file cannot be opened, it will skip
        if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
        {
            return;
        }

        QTextStream stream{ &file };

        std::size_t lineNumber{ 0 };

        // Decide whether uppercase/lowercase differences matter.
        const auto caseSensitivity =
            options.caseSensitive
            ? Qt::CaseSensitive
            : Qt::CaseInsensitive;

        // Read the file one line at a time.
        while (!stream.atEnd())
        {
            // Stop quickly if the user pressed Cancel.
            if (cancelRequested.load(std::memory_order_relaxed))
            {
                return;
            }

            const auto line = stream.readLine();
            ++lineNumber;

            if (line.contains(
                options.query,
                caseSensitivity))
            {
                matches.push_back({
                    .filePath =
                        QString::fromStdString(path.string()),
                    .lineNumber = lineNumber,
                    .lineText = line.trimmed()
                    });
            }
        }
    }
}

    TextSearchResult TextSearcher::search(
        const TextSearchOptions& options,
        const std::atomic_bool& cancelRequested
    )
{
    // The main search Function
    // make sure a folder was selecvted
    if (options.folderPath.trimmed().isEmpty())
    {
        return {
            {},
            QStringLiteral(
                "Please select a folder."
            )
        };
    }

    // Make sure there is actually something to search for.
    if (options.query.isEmpty())
    {
        return {
            {},
            QStringLiteral(
                "Please enter text to search for."
            )
        };
    }//^^^ this converts the QT "qstring" folder into a C++ file system. 

    const std::filesystem::path root{
        options.folderPath.toStdString()
    };

    // Confirm the selected path actually exists and is a directory.
    if (!std::filesystem::exists(root) ||
        !std::filesystem::is_directory(root))
    {
        return {
            {},
            QStringLiteral(
                "The selected folder does not exist."
            )
        };
    }

    const auto extensions =
        parseExtensions(options.fileTypes);

    std::vector<TextSearchMatch> matches;

    try
    {
        if (options.recursive)
        {
            // the key loop 
            for (const auto& entry :
                std::filesystem::recursive_directory_iterator{
                    root,
                    std::filesystem::directory_options::
                        skip_permission_denied
                })
            {
                // Stop searching new files if Cancel was pressed.
                if (cancelRequested.load(std::memory_order_relaxed))
                {
                    break;
                }

                if (!entry.is_regular_file())
                {
                    continue;
                }

                if (!shouldSearchFile(
                    entry.path(),
                    extensions))
                {
                    continue;
                }

                searchFile(
                    entry.path(),
                    options,
                    matches,
                    cancelRequested
                );
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
                if (cancelRequested.load(std::memory_order_relaxed))
                {
                    break;
                }

                if (!entry.is_regular_file())
                {
                    continue;
                }

                if (!shouldSearchFile(
                    entry.path(),
                    extensions))
                {
                    continue;
                }

                searchFile(
                    entry.path(),
                    options,
                    matches,
                    cancelRequested
                );
                // ^^^ Finally this means "Okay, this is what i care about - search ignore it". 
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return {
            {},
            QStringLiteral(
                "An error occurred while searching the folder."
            )
        };
    }// an error occurred while searhing the folder. 

    return {
        std::move(matches),
        {}
    };
    // ^^^ "Here are all the searhes we found, there was no error
}
