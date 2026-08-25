#include "HashCalculator.hpp"

#include <QCryptographicHash>
#include <QFile>

namespace
{
    constexpr qint64 bufferSize{
        64 * 1024
    };

    [[nodiscard]]
    QCryptographicHash::Algorithm toQtAlgorithm(
        const HashAlgorithm algorithm
    )
    {
        switch (algorithm)
        {
        case HashAlgorithm::Sha256:
            return QCryptographicHash::Sha256;

        case HashAlgorithm::Sha512:
            return QCryptographicHash::Sha512;

        case HashAlgorithm::Md5:
            return QCryptographicHash::Md5;
        }

        return QCryptographicHash::Sha256;
    }
}

HashResult HashCalculator::calculate(
    const QString& filePath,
    const HashAlgorithm algorithm
)
{
    QFile file{ filePath };

    if (!file.open(QIODevice::ReadOnly))
    {
        return {
            {},
            QStringLiteral("Unable to open the selected file.")
        };
    }

    QCryptographicHash hasher{
        toQtAlgorithm(algorithm)
    };

    while (!file.atEnd())
    {
        const auto data = file.read(bufferSize);

        if (data.isEmpty() && file.error() != QFile::NoError)
        {
            return {
                {},
                QStringLiteral("An error occurred while reading the file.")
            };
        }

        hasher.addData(data);
    }

    return {
        QString::fromLatin1(
            hasher.result().toHex()
        ),
        {}
    };
}