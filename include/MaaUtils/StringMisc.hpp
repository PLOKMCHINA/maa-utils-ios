#pragma once

#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "MaaUtils/Conf.h"

MAA_NS_BEGIN

template <typename StringT, typename CharT = std::ranges::range_value_t<StringT>>
concept IsSomeKindOfString = std::same_as<CharT, char> || std::same_as<CharT, wchar_t>;

template <typename StringT>
using StringViewT = std::basic_string_view<std::ranges::range_value_t<StringT>>;

template <typename StringT>
requires IsSomeKindOfString<StringT>
inline void string_replace_all_(StringT& str, StringViewT<StringT> from, StringViewT<StringT> to)
{
    for (size_t pos = str.find(from); pos != StringT::npos; pos = str.find(from, pos + to.size())) {
        str.replace(pos, from.size(), to);
    }
}

template <typename StringT, typename MapT>
requires IsSomeKindOfString<StringT>
inline void string_replace_all_(StringT& str, const MapT& replace_map)
{
    // TODO: better algorithm
    for (const auto& [from, to] : replace_map) {
        string_replace_all_(str, from, to);
    }
}

template <typename StringT>
requires IsSomeKindOfString<StringT>
[[nodiscard]] inline auto string_replace_all(StringT&& str, StringViewT<StringT> from, StringViewT<StringT> to)
{
    // TODO: better algorithm
    std::decay_t<StringT> result = std::forward<StringT>(str);
    string_replace_all_(result, from, to);
    return result;
}

template <typename StringT, typename MapT>
requires IsSomeKindOfString<StringT>
[[nodiscard]] inline auto string_replace_all(StringT&& str, const MapT& replace_map)
{
    // TODO: better algorithm
    std::decay_t<StringT> result = std::forward<StringT>(str);
    string_replace_all_(result, replace_map);
    return result;
}

template <typename StringT>
requires IsSomeKindOfString<StringT>
inline void string_trim_(StringT& str)
{
    // 去除所有 Unicode 空白：ASCII 控制字 + space + 全角空格 U+3000 + 零宽空格 U+200B 等
    auto is_wspace = [](auto c) -> bool {
        using U = std::make_unsigned_t<decltype(c)>;
        U uc = static_cast<U>(c);
        // ASCII: 0x09-0x0D (\t\n\v\f\r), 0x20 (space), 0x85 (NEL), 0xA0 (NBSP)
        // 常见 Unicode 空白: U+1680, U+2000-U+200A, U+2028, U+2029, U+202F, U+205F, U+3000
        return (uc >= 0x09 && uc <= 0x0D) || uc == 0x20 || uc == 0x85 || uc == 0xA0 ||
               uc == 0x1680 || (uc >= 0x2000 && uc <= 0x200A) || uc == 0x2028 || uc == 0x2029 ||
               uc == 0x202F || uc == 0x205F || uc == 0x3000;
    };
    auto not_wspace = [&](auto c) { return !is_wspace(c); };

    str.erase(std::ranges::find_if(str | std::views::reverse, not_wspace).base(), str.end());
    str.erase(str.begin(), std::ranges::find_if(str, not_wspace));
}

template <typename StringT>
requires IsSomeKindOfString<StringT>
inline void tolowers_(StringT& str)
{
    using CharT = std::ranges::range_value_t<StringT>;
    for (auto& ch : str) {
        ch = static_cast<CharT>(std::tolower(ch));
    }
}

template <typename StringT>
requires IsSomeKindOfString<StringT>
inline void touppers_(StringT& str)
{
    using CharT = std::ranges::range_value_t<StringT>;
    for (auto& ch : str) {
        ch = static_cast<CharT>(std::toupper(ch));
    }
}

template <typename StringT, typename DelimT>
requires IsSomeKindOfString<StringT>
[[nodiscard]] inline std::vector<StringT> string_split(const StringT& str, const DelimT& delim)
{
    std::vector<StringT> result;
    auto views =
        str | std::views::split(delim) | std::views::transform([](auto&& rng) { return std::basic_string_view(rng.begin(), rng.end()); });

    for (auto v : views) {
        result.emplace_back(v);
    }
    return result;
}

MAA_NS_END
