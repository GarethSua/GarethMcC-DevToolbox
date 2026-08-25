#include "LogViewer.hpp"

#include <QFile>
#include <QTextStream>

namespace
{
    // Work out what type of log message this line contains.
    [[nodiscard]]
    LogLevel detectLevel(const QString& line)
    {
        if (line.contains(
            QStringLiteral("error"),
            Qt::CaseInsensitive))
        {
            return LogLevel::Error;
        }

        if (line.contains(
            QStringLiteral("warning"),
            Qt::CaseInsensitive) ||
            line.contains(
                QStringLiteral("warn"),
                Qt::CaseInsensitive))
        {
            return LogLevel::Warning;
        }

        if (line.contains(
            QStringLiteral("info"),
            Qt::CaseInsensitive))
        {
            return LogLevel::Info;
        }

        return LogLevel::Other;
    }
}

LogLoadResult LogViewer::load(
    const QString& filePath
)
{
    if (filePath.trimmed().isEmpty())
    {
        return {
            {},
            QStringLiteral("Please select a log file.")
        };
    }

    QFile file{ filePath };

    if (!file.open(
        QIODevice::ReadOnly |
        QIODevice::Text))
    {
        return {
            {},
            QStringLiteral(
                "Unable to open the selected log file."
            )
        };
    }

    QTextStream stream{ &file };

    std::vector<LogEntry> entries;

    std::size_t lineNumber{ 0 };

    // read the log one line at a time.
    while (!stream.atEnd())
    {
        const auto line = stream.readLine();
        ++lineNumber;

        entries.push_back({
            .lineNumber = lineNumber,
            .level = detectLevel(line),
            .message = line
            });
    }

    return {
        std::move(entries),
        {}
    };
}

QString LogViewer::levelToString(
    const LogLevel level
)
{
    switch (level)
    {
    case LogLevel::Info:
        return QStringLiteral("INFO");

    case LogLevel::Warning:
        return QStringLiteral("WARNING");

    case LogLevel::Error:
        return QStringLiteral("ERROR");

    case LogLevel::Other:
        return QStringLiteral("OTHER");
    }

    return QStringLiteral("OTHER");
}