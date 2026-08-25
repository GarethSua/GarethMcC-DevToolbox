#include "PasswordGenerator.hpp"

#include <QRandomGenerator>

#include <algorithm>
#include <random>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view uppercaseCharacters{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    };

    constexpr std::string_view lowercaseCharacters{
        "abcdefghijklmnopqrstuvwxyz"
    };

    constexpr std::string_view numberCharacters{
        "0123456789"
    };

    constexpr std::string_view symbolCharacters{
        R"(!@#$%^&*()-_=+[]{};:,.?)"
    };

    [[nodiscard]]
    char randomCharacter(
        const std::string_view characters,
        QRandomGenerator& generator
    )
    {
        std::uniform_int_distribution<std::size_t> distribution{
            0,
            characters.size() - 1
        };

        return characters[distribution(generator)];
    }
}

std::optional<std::string> PasswordGenerator::generate(
    const PasswordOptions& options
)
{
    std::vector<std::string_view> enabledCharacterSets;
    enabledCharacterSets.reserve(4);

    if (options.includeUppercase)
    {
        enabledCharacterSets.push_back(uppercaseCharacters);
    }

    if (options.includeLowercase)
    {
        enabledCharacterSets.push_back(lowercaseCharacters);
    }

    if (options.includeNumbers)
    {
        enabledCharacterSets.push_back(numberCharacters);
    }

    if (options.includeSymbols)
    {
        enabledCharacterSets.push_back(symbolCharacters);
    }

    if (enabledCharacterSets.empty() ||
        options.length < enabledCharacterSets.size())
    {
        return std::nullopt;
    }

    std::string allCharacters;

    for (const auto characterSet : enabledCharacterSets)
    {
        allCharacters.append(characterSet);
    }

    auto& generator = *QRandomGenerator::system();

    std::string password;
    password.reserve(options.length);

    // Guarantee one character from every selected category.
    for (const auto characterSet : enabledCharacterSets)
    {
        password.push_back(
            randomCharacter(characterSet, generator)
        );
    }

    while (password.size() < options.length)
    {
        password.push_back(
            randomCharacter(allCharacters, generator)
        );
    }

    std::shuffle(
        password.begin(),
        password.end(),
        generator
    );

    return password;
}