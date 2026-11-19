/**
 * @file alnativeimtranslationpolicy.h
 * @brief Message and single-flight rules for native IM translation.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace al::native_im_translation
{
struct TextSegment
{
    std::string text;
    bool protected_text;
};

namespace detail
{
inline bool asciiSpace(char ch) noexcept
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

inline bool asciiWord(char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

inline std::uint32_t codePoint(std::string_view text, std::size_t offset, std::size_t& length) noexcept
{
    const auto lead = static_cast<unsigned char>(text[offset]);
    length = 1;
    if (lead < 0x80) return lead;
    std::size_t count = 0;
    std::uint32_t point = 0;
    if (lead >= 0xC2 && lead <= 0xDF) { count = 2; point = lead & 0x1F; }
    else if (lead >= 0xE0 && lead <= 0xEF) { count = 3; point = lead & 0x0F; }
    else if (lead >= 0xF0 && lead <= 0xF4) { count = 4; point = lead & 0x07; }
    if (!count || count > text.size() - offset) return lead;
    for (std::size_t i = 1; i < count; ++i)
    {
        const auto ch = static_cast<unsigned char>(text[offset + i]);
        if ((ch & 0xC0) != 0x80) return lead;
        point = (point << 6) | (ch & 0x3F);
    }
    if ((count == 3 && point < 0x800) || (count == 4 && point < 0x10000) ||
        point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) return lead;
    length = count;
    return point;
}

inline bool unicodeSpace(std::uint32_t point) noexcept
{
    return point == 0x20 || (point >= 0x09 && point <= 0x0D) || point == 0x85 ||
           point == 0xA0 || point == 0x1680 || (point >= 0x2000 && point <= 0x200A) ||
           point == 0x2028 || point == 0x2029 || point == 0x202F || point == 0x205F || point == 0x3000;
}

inline bool translatablePoint(std::uint32_t point) noexcept
{
    if (point < 0x80) return (point >= 'a' && point <= 'z') || (point >= 'A' && point <= 'Z');
    if (unicodeSpace(point)) return false;
    // Punctuation, symbols and emoji alone do not establish a source language.
    if (point == 0xA1 || point == 0xAB || point == 0xBB || point == 0xBF || point == 0xB7 ||
        point == 0xD7 || point == 0xF7 || (point >= 0x2000 && point <= 0x206F) ||
        (point >= 0x2190 && point <= 0x2BFF) || (point >= 0x2E00 && point <= 0x2E7F) ||
        (point >= 0x3000 && point <= 0x303F) || (point >= 0xFE00 && point <= 0xFE1F) ||
        (point >= 0xFE30 && point <= 0xFE4F) || point == 0xFEFF ||
        (point >= 0xFF01 && point <= 0xFF20) || (point >= 0xFF3B && point <= 0xFF40) ||
        (point >= 0xFF5B && point <= 0xFF65) || (point >= 0x1F000 && point <= 0x1FAFF) ||
        (point >= 0xE0020 && point <= 0xE007F)) return false;
    return true;
}

inline bool startsIgnoringCase(std::string_view text, std::size_t offset,
                               std::string_view prefix) noexcept
{
    if (offset > text.size() || prefix.size() > text.size() - offset) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        char ch = text[offset + i];
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (ch != prefix[i]) return false;
    }
    return true;
}

inline bool urlStart(std::string_view text, std::size_t offset) noexcept
{
    constexpr std::string_view prefixes[] = {"mailto:", "tel:", "sip:", "www."};
    for (const auto prefix : prefixes)
    {
        if (startsIgnoringCase(text, offset, prefix)) return true;
    }
    // The viewer recognizes additional protocols such as x-grid-location-info.
    // Follow URI scheme grammar instead of maintaining an incomplete allowlist.
    if (offset >= text.size() || !((text[offset] >= 'a' && text[offset] <= 'z') ||
        (text[offset] >= 'A' && text[offset] <= 'Z'))) return false;
    auto end = offset + 1;
    while (end < text.size() && (asciiWord(text[end]) || text[end] == '+' || text[end] == '-' || text[end] == '.'))
    {
        if (text[end] == '_') return false; // Underscores are not valid scheme characters.
        ++end;
    }
    return text.substr(end, 3) == "://";
}

inline bool emailLocal(char ch) noexcept
{
    return asciiWord(ch) || std::string_view(".!#$%&'*+-/=?^`{|}~").find(ch) != std::string_view::npos;
}

inline std::size_t emailEnd(std::string_view text, std::size_t offset) noexcept
{
    if (!emailLocal(text[offset]) || text[offset] == '.' ||
        (offset && emailLocal(text[offset - 1]))) return 0;
    auto at = offset;
    while (at < text.size() && emailLocal(text[at])) ++at;
    if (at == offset || at >= text.size() || text[at] != '@' || text[at - 1] == '.' ||
        text.substr(offset, at - offset).find("..") != std::string_view::npos) return 0;
    auto end = at + 1;
    while (end < text.size() && (asciiWord(text[end]) || text[end] == '-' || text[end] == '.')) ++end;
    while (end > at + 1 && text[end - 1] == '.') --end;
    auto label = at + 1;
    bool dotted = false;
    for (auto i = label; i <= end; ++i)
    {
        if (i != end && text[i] != '.') continue;
        if (i == label || text[label] == '-' || text[i - 1] == '-' ||
            text.substr(label, i - label).find('_') != std::string_view::npos) return 0;
        if (i == end) return dotted && i - label >= 2 ? end : 0;
        dotted = true;
        label = i + 1;
    }
    return false;
}

inline std::size_t codeEnd(std::string_view text, std::size_t offset) noexcept
{
    const char marker = text[offset];
    if (marker != '`' && marker != '~') return 0;
    auto length = std::size_t{1};
    while (offset + length < text.size() && text[offset + length] == marker) ++length;
    if (marker == '~' && length < 3) return 0;
    for (auto candidate = offset + length; candidate < text.size();)
    {
        if (text[candidate] != marker)
        {
            ++candidate;
            continue;
        }
        auto closing_length = std::size_t{1};
        while (candidate + closing_length < text.size() && text[candidate + closing_length] == marker)
        {
            ++closing_length;
        }
        if ((length >= 3 && closing_length >= length) || (length < 3 && closing_length == length))
        {
            return candidate + closing_length;
        }
        candidate += closing_length;
    }
    // An unfinished code span/fence is still code. Never send it to the
    // translator simply because the person omitted the final delimiter.
    return text.size();
}

inline std::size_t urlEnd(std::string_view text, std::size_t offset) noexcept
{
    // A punctuation mark or unmatched parenthesis can be a real URI path byte.
    // Without an explicit wrapper there is no reliable prose/URI distinction.
    // Preserve the entire token, including attached ambiguous prose, rather
    // than allow a translation to change any part of a possible destination.
    auto end = offset;
    while (end < text.size())
    {
        std::size_t length = 0;
        const auto point = codePoint(text, end, length);
        if (unicodeSpace(point) || point < 0x20 || point == 0x7F) break;
        end += length;
    }
    return end;
}

inline std::size_t bracketEnd(std::string_view text, std::size_t offset) noexcept
{
    std::size_t nesting = 0;
    for (std::size_t i = offset; i < text.size(); ++i)
    {
        if (text[i] == '\\')
        {
            if (i + 1 < text.size()) ++i;
            continue;
        }
        if (text[i] == '[') ++nesting;
        else if (text[i] == ']' && --nesting == 0) return i + 1;
    }
    return 0;
}

inline std::size_t markdownEnd(std::string_view text, std::size_t opening_parenthesis) noexcept
{
    std::size_t nesting = 1;
    char quote = 0;
    bool angle = false;
    for (std::size_t i = opening_parenthesis + 1; i < text.size(); ++i)
    {
        const char ch = text[i];
        if (ch == '\\')
        {
            if (i + 1 < text.size()) ++i;
            continue;
        }
        if (quote)
        {
            if (ch == quote) quote = 0;
            continue;
        }
        if (ch == '"' || (ch == '\'' && asciiSpace(text[i - 1])))
        {
            quote = ch;
            continue;
        }
        if (ch == '<') angle = true;
        else if (ch == '>') angle = false;
        else if (!angle && ch == '(') ++nesting;
        else if (!angle && ch == ')' && --nesting == 0) return i + 1;
    }
    return 0;
}

inline std::size_t protectedEnd(std::string_view text, std::size_t offset) noexcept
{
    // Markdown escapes are data. The next scan starts after the escaped marker
    // so an escaped backtick cannot accidentally open a code span.
    if (text[offset] == '\\' && offset + 1 < text.size())
    {
        const auto ch = static_cast<unsigned char>(text[offset + 1]);
        if ((ch >= 0x21 && ch <= 0x2F) || (ch >= 0x3A && ch <= 0x40) ||
            (ch >= 0x5B && ch <= 0x60) || (ch >= 0x7B && ch <= 0x7E)) return offset + 2;
    }
    if (const auto code = codeEnd(text, offset)) return code;
    const auto opening_bracket = text[offset] == '!' && offset + 1 < text.size()
        && text[offset + 1] == '[' ? offset + 1 : offset;
    if (text[opening_bracket] == '[')
    {
        const auto end = bracketEnd(text, opening_bracket);
        if (end)
        {
            auto url = opening_bracket + 1;
            while (url < end && asciiSpace(text[url])) ++url;
            // Viewer-style [secondlife://... label] and other bracketed URLs
            // must preserve the label as well as the destination.
            if (urlStart(text, url)) return end;
            if (end < text.size() && text[end] == '(')
            {
                url = end + 1;
                while (url < text.size() && asciiSpace(text[url])) ++url;
                if (url < text.size() && text[url] == '<') ++url;
                if (urlStart(text, url)) return markdownEnd(text, end);
            }
        }
    }
    if (urlStart(text, offset)) return urlEnd(text, offset);
    if (const auto email = emailEnd(text, offset)) return email;

    // UUIDs identify avatars, inventory and viewer actions; they are data, not
    // prose. Do not match a UUID substring inside a larger ASCII word.
    if (text.size() - offset >= 36 && (!offset || !asciiWord(text[offset - 1])) &&
        (text.size() - offset == 36 || !asciiWord(text[offset + 36])))
    {
        for (std::size_t i = 0; i < 36; ++i)
        {
            const char ch = text[offset + i];
            const bool hyphen = i == 8 || i == 13 || i == 18 || i == 23;
            if (hyphen ? ch != '-' : !((ch >= '0' && ch <= '9') ||
                (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return 0;
        }
        return offset + 36;
    }
    return 0;
}

inline void appendSegment(std::vector<TextSegment>& segments, std::string_view text, bool protected_text)
{
    if (text.empty()) return;
    if (!segments.empty() && segments.back().protected_text == protected_text)
    {
        segments.back().text.append(text);
    }
    else
    {
        segments.push_back({std::string(text), protected_text});
    }
}

inline void appendProse(std::vector<TextSegment>& segments, std::string_view text)
{
    // Newlines are formatting, not translation input. Trim each prose block's
    // surrounding whitespace into protected spans without changing any bytes.
    std::size_t begin = 0;
    while (begin < text.size())
    {
        auto end = begin;
        std::size_t separator_length = 0;
        while (end < text.size())
        {
            std::size_t length = 0;
            const auto point = codePoint(text, end, length);
            if ((point >= 0x09 && point <= 0x0D) || point == 0x85 || point == 0x2028 || point == 0x2029)
            {
                separator_length = length;
                break;
            }
            end += length;
        }
        auto first = begin;
        while (first < end)
        {
            std::size_t length = 0;
            if (!unicodeSpace(codePoint(text, first, length))) break;
            first += length;
        }
        auto last = first;
        bool translatable = false;
        for (auto cursor = first; cursor < end;)
        {
            std::size_t length = 0;
            const auto point = codePoint(text, cursor, length);
            if (!unicodeSpace(point)) last = cursor + length;
            translatable = translatable || translatablePoint(point);
            cursor += length;
        }
        appendSegment(segments, text.substr(begin, first - begin), true);
        const auto prose = text.substr(first, last - first);
        // Isolated ASCII punctuation/numbers cannot benefit from translation
        // and language detection cannot reliably identify a source for them.
        appendSegment(segments, prose, !translatable);
        appendSegment(segments, text.substr(last, end - last), true);
        if (separator_length) appendSegment(segments, text.substr(end, separator_length), true);
        begin = end + separator_length;
    }
}
} // namespace detail

inline std::vector<TextSegment> protectMessageBody(std::string_view body)
{
    std::vector<TextSegment> segments;
    std::size_t prose_begin = 0;
    for (std::size_t offset = 0; offset < body.size();)
    {
        const auto end = detail::protectedEnd(body, offset);
        if (end > offset)
        {
            detail::appendProse(segments, body.substr(prose_begin, offset - prose_begin));
            detail::appendSegment(segments, body.substr(offset, end - offset), true);
            offset = end;
            prose_begin = offset;
        }
        else ++offset;
    }
    detail::appendProse(segments, body.substr(prose_begin));
    return segments;
}

struct PreparedMessage
{
    std::string prefix;
    std::string body;

    std::string assemble(std::string_view translated) const
    {
        std::string message = prefix;
        message.append(translated);
        return message;
    }
};

inline PreparedMessage prepareMessage(std::string_view message)
{
    // These are the exact emote markers recognized by the viewer. Keep them
    // outside the translated text so the result remains an emote on the wire.
    const bool emote = message.substr(0, 4) == "/me " ||
                       message.substr(0, 4) == "/me'";
    return {std::string(emote ? message.substr(0, 4) : std::string_view{}),
            std::string(emote ? message.substr(4) : message)};
}

inline std::string formatOutgoingEcho(std::string_view original, std::string_view sent)
{
    // This is local display text only. The transport must receive `sent`
    // separately; applying wire limits here would discard the original draft.
    if (original.empty() || original == sent) return std::string(sent);
    if (sent.empty()) return std::string(original);
    auto annotation = sent;
    const auto prefix = original.substr(0, 4);
    if ((prefix == "/me " || prefix == "/me'") && sent.substr(0, 4) == prefix)
    {
        annotation.remove_prefix(4);
    }
    // Empty/prefix-only sent text is not a valid translated send, but keep
    // local formatting benign if a caller nevertheless supplies one.
    if (annotation.empty()) return std::string(original);
    std::string echo(original);
    echo += "\n[";
    echo.append(annotation);
    echo += ']';
    return echo;
}

inline bool isValidResult(const PreparedMessage& message, std::string_view translated,
                          std::size_t max_bytes) noexcept
{
    // Measure bytes, matching the viewer's UTF-8 transport limit. Sending a
    // partial translation would silently change its meaning, so never truncate.
    if (message.prefix.size() > max_bytes ||
        translated.size() > max_bytes - message.prefix.size() ||
        translated.find('\0') != std::string_view::npos)
    {
        return false;
    }
    return translated.find_first_not_of(" \t\r\n\f\v") != std::string_view::npos;
}

// One pending draft per conversation prevents asynchronous results from
// reversing send order. The caller retains the draft until success and sends
// only after finish() accepts the active request. No destructor sends anything.
class RequestSlot
{
public:
    bool begin(std::uint64_t request_id) noexcept
    {
        if (mPending || request_id == 0 || request_id <= mLastRequest)
        {
            return false;
        }
        mLastRequest = request_id;
        mPending = true;
        return true;
    }

    bool accepts(std::uint64_t request_id) const noexcept
    {
        return mPending && request_id == mLastRequest;
    }

    bool finish(std::uint64_t request_id) noexcept
    {
        if (!accepts(request_id))
        {
            return false;
        }
        mPending = false;
        return true;
    }

    void cancel() noexcept { mPending = false; }
    bool pending() const noexcept { return mPending; }
    std::uint64_t id() const noexcept { return mPending ? mLastRequest : 0; }

private:
    std::uint64_t mLastRequest = 0;
    bool mPending = false;
};
} // namespace al::native_im_translation
