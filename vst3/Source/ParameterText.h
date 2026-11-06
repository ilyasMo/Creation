#pragma once
#include <cmath>
#include <cstdlib>
#include <string>

namespace afterglow
{
inline bool parseParameterText(std::string text, const std::string& unit, double& result)
{
    for (auto& ch : text) if (ch == ',') ch = '.';
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || !std::isfinite(value)) return false;
    std::string suffix(end);
    const auto start = suffix.find_first_not_of(" \t\r\n");
    suffix = start == std::string::npos ? "" : suffix.substr(start);
    const auto finish = suffix.find_last_not_of(" \t\r\n");
    if (finish != std::string::npos) suffix.resize(finish + 1);
    for (auto& ch : suffix) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    auto expected = unit;
    for (auto& ch : expected) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    double scale = 1.0;
    if (expected == "hz" && (suffix == "k" || suffix == "khz")) scale = 1000.0;
    else if (!suffix.empty() && suffix != expected) return false;
    result = value * scale;
    return std::isfinite(result);
}
}
