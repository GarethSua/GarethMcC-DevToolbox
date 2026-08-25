#pragma once

#include <QString>

#include <cstddef>
#include <vector>

enum class LogLevel
{
    Info,
    Warning,
    Error,
    Other
};

// this will represent one line from the log file
struct LogEntry final
{
    std::size_t lineNumber{};
    LogLevel level{ LogLevel::Other };
    QString message;
};

// contains either the loaded entries or an error
struct LogLoadResult final
{
    std::vector<LogEntry> entries;
    QString error;

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class LogViewer final
{
public:
    [[nodiscard]]
    static LogLoadResult load(
        const QString& filePath
    );

    [[nodiscard]]
    static QString levelToString(
        LogLevel level
    );
};
