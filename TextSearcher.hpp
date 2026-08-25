#pragma once
#include <QString>
#include <cstddef>
#include <vector>
#include <atomic>

// Stores all of the options chosen by the user in the UI.
struct TextSearchOptions final
{
    QString folderPath;
    QString query;
    QString fileTypes;

    bool recursive{ true };
    bool caseSensitive{ false };
};

// Represents one match found inside a file.
struct TextSearchMatch final
{
    QString filePath;
    std::size_t lineNumber{};
    QString lineText;
};

// Contains either the matches or an error message.
struct TextSearchResult final
{
    std::vector<TextSearchMatch> matches;
    QString error;

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class TextSearcher final
{
public:
    // Searches the folder using the supplied options.
    [[nodiscard]]
    static TextSearchResult search(
        const TextSearchOptions& options,
        const std::atomic_bool& cancelRequested
    );
};


//TextSearchOptions = what should I search?
//TextSearchMatch = what did I find ?
//TextSearchResult = how did the whole search go ?
//TextSearcher = actually performs the search