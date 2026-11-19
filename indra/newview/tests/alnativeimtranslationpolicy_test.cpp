/**
 * @file alnativeimtranslationpolicy_test.cpp
 * @brief Standalone native IM translation message and lifetime contracts.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../alnativeimtranslationpolicy.h"

#include <cstdlib>
#include <iostream>
#include <random>

namespace
{
int checks = 0;
int failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

std::string joined(const std::vector<al::native_im_translation::TextSegment>& segments)
{
    std::string result;
    for (const auto& segment : segments) result += segment.text;
    return result;
}

std::size_t proseCount(const std::vector<al::native_im_translation::TextSegment>& segments)
{
    std::size_t count = 0;
    for (const auto& segment : segments) count += !segment.protected_text;
    return count;
}

void checkSegments(std::string_view source,
                   std::initializer_list<al::native_im_translation::TextSegment> expected,
                   const char* description)
{
    const auto actual = al::native_im_translation::protectMessageBody(source);
    check(joined(actual) == source, description);
    bool exact = actual.size() == expected.size();
    if (exact)
    {
        auto want = expected.begin();
        for (const auto& segment : actual)
        {
            exact = exact && segment.text == want->text && segment.protected_text == want->protected_text;
            ++want;
        }
    }
    check(exact, description);
}
} // namespace

int main()
{
    using namespace al::native_im_translation;

    const auto ordinary = prepareMessage("Hello, friend.");
    check(ordinary.prefix.empty(), "Ordinary text has no emote prefix");
    check(ordinary.body == "Hello, friend.", "Ordinary source is unchanged");
    check(ordinary.assemble("Bonjour, mon ami.") == "Bonjour, mon ami.",
          "Ordinary translated body is sent without added text");
    check(isValidResult(ordinary, ordinary.body, 100),
          "Already-target-language result is accepted unchanged");

    const auto emote = prepareMessage("/me waves hello.");
    check(emote.prefix == "/me ", "Space emote marker is retained");
    check(emote.body == "waves hello.", "Space emote marker is excluded from translation");
    check(emote.assemble("fait signe.") == "/me fait signe.", "Translated space emote remains an emote");
    const auto possessive = prepareMessage("/me's hat is red.");
    check(possessive.prefix == "/me'", "Apostrophe emote marker is retained");
    check(possessive.body == "s hat is red.", "Only the recognized apostrophe marker is stripped");
    check(possessive.assemble("s chapeau est rouge.") == "/me's chapeau est rouge.",
          "Translated apostrophe emote retains its wire marker");
    check(prepareMessage("/message").prefix.empty(), "Other slash text is not mistaken for an emote");
    check(prepareMessage("/me").body == "/me", "Incomplete emote marker is ordinary text");
    check(prepareMessage(" /me hello").prefix.empty(), "Only a leading marker is stripped");
    check(prepareMessage("/ME hello").prefix.empty(), "Emote recognition matches viewer case rules");
    check(prepareMessage("").body.empty(), "Empty source can be inspected safely");

    check(!isValidResult(ordinary, "", 5119), "An empty result cannot send");
    check(!isValidResult(ordinary, " \t\r\n\f\v", 5119), "A blank result cannot send");
    check(!isValidResult(ordinary, std::string("hi\0there", 8), 5119),
          "Embedded NUL cannot silently shorten a wire message");
    check(isValidResult(ordinary, std::string(5119, 'a'), 5119), "Exact transport byte limit is accepted");
    check(!isValidResult(ordinary, std::string(5120, 'a'), 5119), "Oversized output is rejected intact");
    check(isValidResult(emote, std::string(5115, 'a'), 5119), "Emote prefix counts toward transport bytes");
    check(!isValidResult(emote, std::string(5116, 'a'), 5119), "Emote prefix cannot overflow transport limit");
    check(!isValidResult(emote, "a", 3), "Prefix larger than the limit cannot underflow byte guard");
    const std::string unicode = "こんにちは";
    check(unicode.size() == 15, "UTF-8 fixture measures bytes rather than characters");
    check(isValidResult(ordinary, unicode, 15), "Complete Unicode output at byte limit is accepted");
    check(!isValidResult(ordinary, unicode, 14), "Unicode output beyond limit is rejected without slicing");
    check(ordinary.assemble(unicode) == unicode, "Unicode translation bytes survive assembly");

    RequestSlot first_chat;
    RequestSlot second_chat;
    check(!first_chat.pending() && first_chat.id() == 0, "New chat starts idle");
    check(!first_chat.begin(0), "Zero is reserved for no request");
    check(first_chat.begin(10), "First outgoing translation starts");
    check(first_chat.pending() && first_chat.id() == 10, "Active request identity is retained");
    check(!first_chat.begin(11), "Second send cannot overtake a pending draft");
    check(second_chat.begin(11), "Another conversation remains independent");
    check(!first_chat.accepts(11) && first_chat.accepts(10), "A result belongs to its exact pending request");
    check(!first_chat.finish(9) && first_chat.pending(), "Stale completion cannot unlock a current draft");
    check(first_chat.finish(10) && !first_chat.pending(), "Current result completes once");
    check(!first_chat.finish(10), "Duplicate completion is ignored");
    check(!first_chat.begin(10) && !first_chat.begin(9), "Completed identifiers cannot be reused");
    check(first_chat.begin(12), "Next monotonic request starts after completion");
    first_chat.cancel();
    check(!first_chat.pending() && first_chat.id() == 0, "Cancellation leaves no active request");
    check(!first_chat.accepts(12) && !first_chat.finish(12), "Late canceled result cannot send");
    check(!first_chat.begin(12), "Cancellation does not make the old identifier reusable");
    check(first_chat.begin(13), "New draft can start after cancellation");
    check(!first_chat.finish(12) && first_chat.accepts(13), "Late canceled result cannot complete the replacement");
    check(second_chat.accepts(11), "Canceling one chat does not affect another");
    check(first_chat.finish(13) && second_chat.finish(11), "Both chats complete their own current requests");

    checkSegments("", {}, "Empty text yields no segments");
    checkSegments("Hello, friend.", {{"Hello, friend.", false}}, "Ordinary prose remains one translation unit");
    checkSegments("  Hello, friend.  ", {{"  ", true}, {"Hello, friend.", false}, {"  ", true}},
                  "Whitespace around prose stays verbatim");
    checkSegments("Hello\nworld\r\nagain\tplease",
                  {{"Hello", false}, {"\n", true}, {"world", false}, {"\r\n", true},
                   {"again", false}, {"\t", true}, {"please", false}},
                  "Line endings and tabs never enter translation");
    checkSegments("\u00a0Bonjour\u2003", {{"\u00a0", true}, {"Bonjour", false}, {"\u2003", true}},
                  "Unicode whitespace around prose stays verbatim");
    checkSegments("Bonjour\u2028你好", {{"Bonjour", false}, {"\u2028", true}, {"你好", false}},
                  "Unicode line separator is formatting");
    checkSegments("Visit https://example.com/a?q=1&b=2 now.",
                  {{"Visit", false}, {" https://example.com/a?q=1&b=2 ", true}, {"now.", false}},
                  "Only prose surrounding an HTTPS URL enters translation");
    checkSegments("See https://example.com/path_(thing)). Next.",
                  {{"See", false}, {" https://example.com/path_(thing)). ", true}, {"Next.", false}},
                  "Ambiguous attached URL punctuation stays protected rather than being rewritten");
    checkSegments("See https://example.com, please.",
                  {{"See", false}, {" https://example.com, ", true}, {"please.", false}},
                  "Attached comma stays protected while following separate prose is translated");
    checkSegments("Here:https://example.com/a%20b#fragment",
                  {{"Here:", false}, {"https://example.com/a%20b#fragment", true}},
                  "A URL is protected next to prose punctuation without a space");
    checkSegments("你好https://example.com/こんにちは。下一句",
                  {{"你好", false}, {"https://example.com/こんにちは。下一句", true}},
                  "Ambiguous Unicode suffix stays protected with a possible internationalized URI");
    checkSegments("Visit https://example.com/日本、語 now.",
                  {{"Visit", false}, {" https://example.com/日本、語 ", true}, {"now.", false}},
                  "Internal Japanese punctuation remains an exact URI path byte");
    checkSegments("Visit https://example.com/foo)bar now.",
                  {{"Visit", false}, {" https://example.com/foo)bar ", true}, {"now.", false}},
                  "Unmatched closing parenthesis inside a URI cannot expose its suffix to translation");
    checkSegments("Visit https://example.com/a,b;still!path?x=(value)). now.",
                  {{"Visit", false}, {" https://example.com/a,b;still!path?x=(value)). ", true}, {"now.", false}},
                  "URI punctuation and query bytes remain protected through the token boundary");
    checkSegments("https://example.com/path\u00a0Bonjour",
                  {{"https://example.com/path\u00a0", true}, {"Bonjour", false}},
                  "Unicode whitespace ends the URI token without losing separator bytes");
    checkSegments("Use 01234567-89ab-CDEF-0123-456789abcdef here.",
                  {{"Use", false}, {" 01234567-89ab-CDEF-0123-456789abcdef ", true}, {"here.", false}},
                  "Avatar and inventory UUIDs remain data");
    checkSegments("x01234567-89ab-cdef-0123-456789abcdefz",
                  {{"x01234567-89ab-cdef-0123-456789abcdefz", false}},
                  "UUID-shaped substring inside an ASCII word is ordinary prose");
    checkSegments("[secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about Jane Doe]",
                  {{"[secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about Jane Doe]", true}},
                  "Bracketed viewer action preserves its URL and label together");
    checkSegments("[secondlife://Ahern/128/128/20 Nice place]",
                  {{"[secondlife://Ahern/128/128/20 Nice place]", true}},
                  "Bracketed SLURL label never enters translation");
    checkSegments("See [a nice place](https://example.com/path_(thing)) today.",
                  {{"See", false}, {" [a nice place](https://example.com/path_(thing)) ", true}, {"today.", false}},
                  "Markdown label and balanced destination remain one protected block");
    checkSegments("[read [nested]](HTTPS://Example.com)", {{"[read [nested]](HTTPS://Example.com)", true}},
                  "Nested Markdown label brackets remain intact");
    checkSegments("[read \\] more](https://example.com)", {{"[read \\] more](https://example.com)", true}},
                  "Escaped Markdown label bracket remains intact");
    checkSegments("[site](<https://example.com/path_(thing)> \"A title with )\")",
                  {{"[site](<https://example.com/path_(thing)> \"A title with )\")", true}},
                  "Markdown angle destination and quoted title stay intact");
    checkSegments("![image label](https://example.com/a.png)", {{"![image label](https://example.com/a.png)", true}},
                  "Markdown image URL and alt label stay intact");
    checkSegments("Open x-grid-location-info://grid.example/region/128/128/20 now.",
                  {{"Open", false}, {" x-grid-location-info://grid.example/region/128/128/20 ", true}, {"now.", false}},
                  "Actual viewer grid-location protocol never enters translation");
    checkSegments("Open Viewer+Action.V2://app/agent/01234567-89ab-cdef-0123-456789abcdef/about now.",
                  {{"Open", false}, {" Viewer+Action.V2://app/agent/01234567-89ab-cdef-0123-456789abcdef/about ", true},
                   {"now.", false}}, "A valid arbitrary scheme protects direct action data");
    checkSegments("Email foo+tag@example.com, please.",
                  {{"Email", false}, {" foo+tag@example.com", true}, {", please.", false}},
                  "Raw tagged email remains exact data rather than prose");
    checkSegments("Use `name(\"Hello\")` here.",
                  {{"Use", false}, {" `name(\"Hello\")` ", true}, {"here.", false}},
                  "Inline code and its string literals never enter translation");
    checkSegments("Use ``text `quoted` more`` here.",
                  {{"Use", false}, {" ``text `quoted` more`` ", true}, {"here.", false}},
                  "Double backtick code can contain literal single backticks");
    checkSegments("Before\n```swift\nlet greeting = \"Hello\"\n```\nAfter",
                  {{"Before", false}, {"\n```swift\nlet greeting = \"Hello\"\n```\n", true}, {"After", false}},
                  "Fenced code language, contents and formatting stay intact");
    checkSegments("Before\n~~~\ncode with `backticks`\n~~~\nAfter",
                  {{"Before", false}, {"\n~~~\ncode with `backticks`\n~~~\n", true}, {"After", false}},
                  "Tilde fenced code stays intact");
    checkSegments("See `unfinished code", {{"See", false}, {" `unfinished code", true}},
                  "Unfinished inline code is conservatively protected through end");
    checkSegments("Use \\`name\\` here.",
                  {{"Use", false}, {" \\`", true}, {"name", false}, {"\\` ", true}, {"here.", false}},
                  "Escaped backticks preserve their escape and do not open code");

    const std::string link_only[] = {
        "http://example.com", "HTTPS://Example.com", "FtP://example.com/file.txt",
        "secondlife://Ahern/128/128/20", "HoP://grid.example/Ahern/128/128/20",
        "X-GRID-INFO://grid.example", "MaIlTo:alice@example.com?subject=Hello", "WWW.Example.com/a",
        "x-grid-location-info://grid.example/Ahern/128/128/20", "custom+scheme.v2://host/action?id=123",
        "tel:+35312345678", "sip:alice@example.com", "foo+tag@example.com", "<foo+tag@example.com>",
        "`print(\"Hello\")`", "```swift\nprint(\"Hello\")\n```", "~~~\ncode\n~~~", "`unfinished",
        "(https://example.com/a_(b)).", "\"https://example.com\"!", "'https://example.com'",
        "<https://[2001:db8::1]:8080/p?a=b>", "https://example.com.,!?:;",
        "https://example.com/a%28b%29#fragment", "https://a.example https://b.example\nwww.example.com",
        "https://example.com/日本、語", "https://example.com/foo)bar", "https://example.com/こんにちは。下一句",
        "[https://example.com link label]", "01234567-89ab-cdef-0123-456789abcdef",
        " https://example.com。🙂 ", "«https://example.com/こんにちは»", "\u00a0https://example.com\u3000",
        " \r\n\t ", "1234, .!?", "🙂✨!!!"
    };
    for (const auto& source : link_only)
    {
        const auto segments = protectMessageBody(source);
        check(joined(segments) == source, "Link-only text retains every original byte");
        check(proseCount(segments) == 0, "Link-only text requires no translation or source detection");
    }

    const std::string nul_source("Go https://example.com\0 now.", 27);
    const auto nul_segments = protectMessageBody(nul_source);
    check(joined(nul_segments) == nul_source, "Embedded NUL survives segmentation without hidden truncation");
    check(!isValidResult(prepareMessage(nul_source), joined(nul_segments), 5119),
          "Assembled message containing NUL is rejected before wire transport");
    const auto emote_links = prepareMessage("/me visits [secondlife://Ahern/128/128/20 Ahern].");
    check(emote_links.prefix == "/me " &&
          joined(protectMessageBody(emote_links.body)) == "visits [secondlife://Ahern/128/128/20 Ahern].",
          "Emote handling and link segmentation compose without altering action data");

    // Exercise the byte-preserving scanner on malformed/truncated UTF-8 and
    // arbitrary controls as well as valid prose. It must never overrun or lose
    // bytes; final transport validation remains responsible for rejecting NUL.
    std::mt19937 random(0xA1C4E);
    for (int trial = 0; trial < 256; ++trial)
    {
        std::string source;
        for (int i = 0; i < trial; ++i) source += static_cast<char>(random() & 0xFF);
        source.insert(source.size() / 2, "https://example.com/a_(b)");
        const auto segments = protectMessageBody(source);
        check(joined(segments) == source, "Scanner roundtrips arbitrary bytes with an embedded URL");
        bool normalized = true;
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            normalized = normalized && !segments[i].text.empty() &&
                (i == 0 || segments[i - 1].protected_text != segments[i].protected_text);
        }
        check(normalized, "Scanner emits nonempty segments and merges adjacent identical modes");
    }

    std::cout << checks << " native IM translation policy checks; " << failures << " failures\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
