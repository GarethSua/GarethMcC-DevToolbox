#pragma once

#include <QString>

enum class HashAlgorithm
{
    Sha256,
    Sha512,
    Md5
};

struct HashResult final
{
    QString hash;
    QString error;

    [[nodiscard]]
    bool succeeded() const noexcept
    {
        return error.isEmpty();
    }
};

class HashCalculator final
{
public:
    [[nodiscard]]
    static HashResult calculate(
        const QString& filePath,
        HashAlgorithm algorithm
    );
};
