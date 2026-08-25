#pragma once

#include <QString>

struct JsonResult final
{
    QString output;
    QString error;

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class JsonFormatter final
{
public:
    [[nodiscard]]
    static JsonResult format(const QString& input);

    [[nodiscard]]
    static JsonResult minify(const QString& input);

    [[nodiscard]]
    static JsonResult validate(const QString& input);
};
