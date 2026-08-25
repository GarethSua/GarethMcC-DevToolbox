#include "JsonFormatter.hpp"

#include <QJsonParseError>
#include <QJsonValue>

namespace
{
    [[nodiscard]]
    JsonResult parseJson(
        const QString& input,
        const QJsonValue::JsonFormat format
    )
    {
        if (input.trimmed().isEmpty())
        {
            return {
                {},
                QStringLiteral("JSON input is empty.")
            };
        }

        QJsonParseError parseError;

        const auto value = QJsonValue::fromJson(
            input.toUtf8(),
            &parseError
        );

        if (parseError.error != QJsonParseError::NoError)
        {
            return {
                {},
                QStringLiteral("Invalid JSON: %1 at position %2.")
                    .arg(parseError.errorString())
                    .arg(parseError.offset)
            };
        }

        return {
            QString::fromUtf8(value.toJson(format)),
            {}
        };
    }
}

JsonResult JsonFormatter::format(const QString& input)
{
    return parseJson(
        input,
        QJsonValue::JsonFormat::Indented
    );
}

JsonResult JsonFormatter::minify(const QString& input)
{
    return parseJson(
        input,
        QJsonValue::JsonFormat::Compact
    );
}

JsonResult JsonFormatter::validate(const QString& input)
{
    const auto result = parseJson(
        input,
        QJsonValue::JsonFormat::Compact
    );

    if (!result.succeeded())
    {
        return result;
    }

    return {
        QStringLiteral("Valid JSON"),
        {}
    };
}