#pragma once

#include <cstddef>
#include <optional>
#include <string>

struct PasswordOptions final
{
    std::size_t length{ 16 };

    bool includeUppercase{ true };
    bool includeLowercase{ true };
    bool includeNumbers{ true };
    bool includeSymbols{ false };
};

class PasswordGenerator final
{
public:
    [[nodiscard]]
    static std::optional<std::string> generate(
        const PasswordOptions& options
    );
};