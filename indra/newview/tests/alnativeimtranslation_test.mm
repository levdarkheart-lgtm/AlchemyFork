/** Copyright (C) 2026, Alchemy Viewer Project.
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * Standalone real-API regression probe; no viewer, credentials, IM recipient,
 * network translation provider, or automatic model download is involved.
 * Exit 77 means Apple's local language service is unavailable in this context.
 */
#import <AppKit/AppKit.h>
#include "alnativeimtranslation.h"
#include "alnativeimtranslationpolicy.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace
{
std::map<uint64_t, std::string> results;
int failures = 0;
int checks = 0;

void check(bool condition, const char* label)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", label);
    }
}

void complete(uint64_t request, const char* json)
{
    check([NSThread isMainThread], "C ABI completion runs on the main thread");
    check(results.count(request) == 0, "completion occurs once per request");
    results[request] = json ? json : "";
}

bool waitFor(uint64_t request, double seconds = 20.)
{
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while (!results.count(request) && deadline.timeIntervalSinceNow > 0.)
    {
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    return results.count(request) != 0;
}

NSDictionary* result(uint64_t request)
{
    NSData* data = [NSData dataWithBytes:results[request].data() length:results[request].size()];
    id value = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    check([value isKindOfClass:[NSDictionary class]], "completion contains a JSON object");
    return [value isKindOfClass:[NSDictionary class]] ? value : @{};
}

bool errorIs(NSDictionary* value, NSString* code)
{
    return ![value[@"ok"] boolValue] && [value[@"error_code"] isEqual:code];
}

std::string scannerJSON(std::string_view body)
{
    NSMutableArray* entries = [NSMutableArray array];
    for (const auto& segment : al::native_im_translation::protectMessageBody(body))
    {
        NSString* text = [[NSString alloc] initWithBytes:segment.text.data()
                                                length:segment.text.size()
                                              encoding:NSUTF8StringEncoding];
        check(text != nil, "production scanner retains valid UTF-8");
        [entries addObject:@{@"text": text ?: @"", @"protected": @(segment.protected_text)}];
    }
    NSData* data = [NSJSONSerialization dataWithJSONObject:entries options:0 error:nil];
    return std::string(static_cast<const char*>(data.bytes), data.length);
}

std::string resultText(NSDictionary* value)
{
    NSData* data = [value[@"text"] dataUsingEncoding:NSUTF8StringEncoding];
    return data ? std::string(static_cast<const char*>(data.bytes), data.length) : std::string{};
}

void checkProtectedSpans(std::string_view original, std::string_view translated)
{
    std::size_t cursor = 0;
    for (const auto& segment : al::native_im_translation::protectMessageBody(original))
    {
        if (!segment.protected_text) continue;
        const auto position = translated.find(segment.text, cursor);
        check(position != std::string_view::npos, "production-protected span survives byte-identically in order");
        if (position != std::string_view::npos) cursor = position + segment.text.size();
    }
    check(std::count(original.begin(), original.end(), '\n') == std::count(translated.begin(), translated.end(), '\n'),
          "production scanner preserves exact newline count through native translation");
}
}

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        if (argc > 1 && std::strcmp(argv[1], "--setup") == 0)
        {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            al_native_im_translation_setup(argc > 2 ? argv[2] : "en", argc > 3 ? argv[3] : "es");
            [NSApp activateIgnoringOtherApps:YES];
            [NSApp run];
            al_native_im_translation_shutdown();
            return 0;
        }
        const bool requireLive = argc > 1 && std::strcmp(argv[1], "--live") == 0;
        if (!al_native_im_translation_available())
        {
            std::puts("SKIP: headless Apple Translation requires macOS 26.");
            return 77;
        }

        // These checks also run in a sandbox where Apple's local service may
        // return no languages. Empty text never invokes translation.
        al_native_im_translate(1, "", "en", "es", complete);
        check(waitFor(1), "empty input completes");
        check(errorIs(result(1), @"empty_text"), "empty input is rejected");
        al_native_im_translate_segments(11, "[{\"text\":\"secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about\",\"protected\":true}]", "", "es", complete);
        check(waitFor(11), "protected-only request completes without models");
        NSDictionary* protectedOnly = result(11);
        check([protectedOnly[@"ok"] boolValue] && [protectedOnly[@"text"] isEqual:@"secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about"], "protected-only action link is byte-identical");
        al_native_im_translate_segments(12, "invalid", "en", "es", complete);
        check(waitFor(12), "invalid segmented request completes");
        check(errorIs(result(12), @"invalid_input"), "invalid segmented input is rejected");
        NSMutableArray* excessPieces = [NSMutableArray array];
        for (int index = 0; index < 65; ++index)
        {
            [excessPieces addObject:@{@"text": @"word", @"protected": @NO}];
        }
        NSData* excessJSON = [NSJSONSerialization dataWithJSONObject:excessPieces options:0 error:nil];
        NSString* excessText = [[NSString alloc] initWithData:excessJSON encoding:NSUTF8StringEncoding];
        al_native_im_translate_segments(14, [excessText UTF8String], "en", "es", complete);
        check(waitFor(14), "excess fragment request completes");
        check(errorIs(result(14), @"too_many_segments"), "excess prose fragments fail without translation");
        const std::string linkOnly = "https://example.com/CaSe?x=%2F#Anchor\nsecondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about\n[secondlife://Ahern/128/128/23 東京で会いましょう]\n🙂\n";
        const auto linkJSON = scannerJSON(linkOnly);
        al_native_im_translate_segments(15, linkJSON.c_str(), "", "ja", complete);
        check(waitFor(15), "production scanner link-only native request completes without models");
        NSDictionary* linkResult = result(15);
        check([linkResult[@"ok"] boolValue] && resultText(linkResult) == linkOnly, "production scanner link-only result is byte-identical");
        al_native_im_translation_languages(2, complete);
        al_native_im_translation_cancel(2);
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        check(!results.count(2), "cancel suppresses callback");

        al_native_im_translation_languages(3, complete);
        check(waitFor(3), "language enumeration completes");
        NSDictionary* languages = result(3);
        if (errorIs(languages, @"service_unavailable"))
        {
            al_native_im_translate(99, "Hello.", "en", "es", complete);
            al_native_im_translation_shutdown();
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
            check(!results.count(99), "shutdown suppresses callback");
            std::printf("SKIP: Apple's local language service returned no supported languages; %d checks, %d failures.\n", checks, failures);
            return failures ? 1 : 77;
        }
        check([languages[@"ok"] boolValue], "language enumeration succeeds");
        check([languages[@"languages"] count] > 0, "supported language list is nonempty");
        check([languages[@"system_language"] length] > 0, "system language is explicit");
        std::printf("LANGUAGES %lu\n", static_cast<unsigned long>([languages[@"languages"] count]));

        const char* original = "Hello, how are you doing today?";
        al_native_im_translate(4, original, "en", "en-IE", complete);
        check(waitFor(4), "same-language request completes");
        NSDictionary* identity = result(4);
        std::printf("REGIONAL en->en-IE %s\n", results[4].c_str());
        check([identity[@"ok"] boolValue], "regional target maps to supported English");
        check([identity[@"text"] isEqual:@"Hello, how are you doing today?"], "same-language result preserves exact bytes");
        const char* regionalTargets[] = {"en-US", "en-GB", "ja-JP", "zh-Hans", "zh-Hant"};
        const char* regionalSources[] = {"en", "en", "ja", "zh-CN", "zh-TW"};
        for (uint64_t index = 0; index < 5; ++index)
        {
            al_native_im_translate(20 + index, original, regionalSources[index], regionalTargets[index], complete);
            check(waitFor(20 + index), "regional/script identity completes");
            NSDictionary* regional = result(20 + index);
            check([regional[@"ok"] boolValue] && [regional[@"text"] isEqual:@"Hello, how are you doing today?"], "language script identity ignores irrelevant region");
            std::printf("REGIONAL %s->%s %s\n", regionalSources[index], regionalTargets[index], results[20 + index].c_str());
        }
        al_native_im_translate(25, "汉字简体测试", "zh-Hans", "zh-Hant", complete);
        check(waitFor(25), "different Chinese scripts complete");
        NSDictionary* chinese = result(25);
        check((![chinese[@"ok"] boolValue] && (errorIs(chinese, @"not_installed") || errorIs(chinese, @"unsupported_pair"))) ||
              ([chinese[@"ok"] boolValue] && ![chinese[@"text"] isEqual:@"汉字简体测试"]), "Simplified and Traditional Chinese are not collapsed into identity");
        std::printf("SCRIPTS zh-Hans->zh-Hant %s\n", results[25].c_str());

        al_native_im_translate(5, original, "en", "zz", complete);
        check(waitFor(5), "unsupported target completes");
        check(errorIs(result(5), @"unsupported_target"), "unsupported target is explicit");
        al_native_im_translate(6, original, "zz", "en", complete);
        check(waitFor(6), "unsupported source completes");
        check(errorIs(result(6), @"unsupported_source"), "unsupported source is explicit");

        // A real translation is attempted only with models already installed.
        // Missing packs are a supported experiment result, never an implicit
        // consent to download or a claimed native translation success.
        al_native_im_translate(7, original, "en", "es", complete);
        check(waitFor(7), "real English to Spanish request completes");
        NSDictionary* translated = result(7);
        if ([translated[@"ok"] boolValue])
        {
            check([translated[@"text"] length] > 0, "real translation returns text");
            check(![translated[@"text"] isEqual:@"Hello, how are you doing today?"], "real translation changes English source");
            check([translated[@"source"] isEqual:@"en"] && [translated[@"target"] isEqual:@"es"], "real translation reports requested language pair");
            std::printf("LIVE en->es: %s\n", [translated[@"text"] UTF8String]);
        }
        else
        {
            check(errorIs(translated, @"not_installed"), "missing language packs are explicit");
            check(!requireLive, "--live requires installed packs and a real native translation");
            std::puts("LIVE en->es: not_installed (no models downloaded by test).");
        }

        al_native_im_translate_segments(13, "[{\"text\":\"Hello, please visit \",\"protected\":false},{\"text\":\"https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor\",\"protected\":true},{\"text\":\" after five o'clock.\",\"protected\":false}]", "en", "es", complete);
        check(waitFor(13), "segmented English to Spanish completes");
        NSDictionary* segmented = result(13);
        if ([segmented[@"ok"] boolValue])
        {
            NSString* mixed = segmented[@"text"];
            check([mixed containsString:@" https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor "], "URL bytes and adjacent spaces survive native translation");
            check(![mixed hasPrefix:@"Hello, please visit"], "surrounding prose is translated");
            std::printf("LIVE segmented en->es: %s\n", [mixed UTF8String]);
        }
        else
        {
            check(errorIs(segmented, @"not_installed"), "segmented request reports missing packs");
        }

        al_native_im_translate(26, "Hola, ¿cómo estás hoy?", "", "en", complete);
        check(waitFor(26), "automatic Spanish to English completes");
        NSDictionary* reverse = result(26);
        if ([reverse[@"ok"] boolValue])
        {
            check([reverse[@"source"] isEqual:@"es"] && [reverse[@"target"] isEqual:@"en"], "Spanish source is automatically identified");
            check([reverse[@"text"] length] > 0 && ![reverse[@"text"] isEqual:@"Hola, ¿cómo estás hoy?"], "reverse native translation returns English");
        }
        else
        {
            check(errorIs(reverse, @"not_installed") && !requireLive, "reverse native translation requires installed packs in --live");
        }
        std::printf("LIVE auto es->en %s\n", results[26].c_str());

        const char* shortText[] = {"Hi", "OK", "🙂 123 !"};
        for (uint64_t index = 0; index < 3; ++index)
        {
            al_native_im_translate(27 + index, shortText[index], "en", "es", complete);
            check(waitFor(27 + index), "explicit-source short message completes");
            NSDictionary* shortResult = result(27 + index);
            check(([shortResult[@"ok"] boolValue] && [shortResult[@"text"] length] > 0) ||
                  (!requireLive && errorIs(shortResult, @"not_installed")), "explicit-source short message succeeds or needs packs");
            if (index == 2) check([shortResult[@"text"] isEqual:@"🙂 123 !"], "emoji numbers punctuation are preserved exactly");
            std::printf("SHORT en->es %s\n", results[27 + index].c_str());
        }

        // Japanese is an explicitly requested experiment target. --live
        // requires its installed model and verifies both directions.
        al_native_im_translate(30, original, "en", "ja", complete);
        check(waitFor(30), "English to Japanese completes");
        NSDictionary* japanese = result(30);
        if ([japanese[@"ok"] boolValue])
        {
            check([japanese[@"text"] length] > 0 && ![japanese[@"text"] isEqual:@"Hello, how are you doing today?"], "native Japanese translation changes the source");
            check([japanese[@"source"] isEqual:@"en"] && [japanese[@"target"] isEqual:@"ja"], "native Japanese translation reports requested pair");
        }
        else check(errorIs(japanese, @"not_installed") && !requireLive, "--live requires English Japanese model");
        std::printf("LIVE en->ja %s\n", results[30].c_str());

        al_native_im_translate(31, "こんにちは、お元気ですか？\nまた明日お話ししましょう。", "", "en", complete);
        check(waitFor(31), "automatic Japanese to English completes");
        NSDictionary* japaneseReverse = result(31);
        if ([japaneseReverse[@"ok"] boolValue])
        {
            check([japaneseReverse[@"source"] isEqual:@"ja"] && [japaneseReverse[@"target"] isEqual:@"en"], "Japanese source is automatically identified");
            check([japaneseReverse[@"text"] length] > 0 && ![japaneseReverse[@"text"] containsString:@"こんにちは"], "native Japanese reverse returns translated prose");
        }
        else check(errorIs(japaneseReverse, @"not_installed") && !requireLive, "--live requires automatic Japanese reverse success");
        std::printf("LIVE auto ja->en %s\n", results[31].c_str());

        al_native_im_translate_segments(32, "[{\"text\":\"こんにちは！\\nここを見てください：\",\"protected\":false},{\"text\":\"https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor\",\"protected\":true},{\"text\":\"。\\nまた明日！\",\"protected\":false}]", "ja", "en", complete);
        check(waitFor(32), "Japanese mixed URL punctuation newlines completes");
        NSDictionary* japaneseMixed = result(32);
        if ([japaneseMixed[@"ok"] boolValue])
        {
            check([japaneseMixed[@"text"] containsString:@"https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor"], "URL bytes survive Japanese native translation");
            check(![japaneseMixed[@"text"] containsString:@"こんにちは"], "Japanese surrounding prose is translated");
        }
        else check(errorIs(japaneseMixed, @"not_installed") && !requireLive, "--live requires Japanese mixed URL success");
        std::printf("LIVE segmented ja->en %s\n", results[32].c_str());
        al_native_im_translate(33, "こんにちは", "ja", "en", complete);
        check(waitFor(33), "short Japanese greeting completes");
        NSDictionary* japaneseShort = result(33);
        check(([japaneseShort[@"ok"] boolValue] && [japaneseShort[@"text"] length] > 0 && ![japaneseShort[@"text"] isEqual:@"こんにちは"]) ||
              (!requireLive && errorIs(japaneseShort, @"not_installed")), "explicit short Japanese greeting translates");
        std::printf("SHORT ja->en %s\n", results[33].c_str());

        const std::string mixedJapanese =
            "こんにちは、お元気ですか？\n"
            "詳細はこちら：https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor。\n"
            "🙂\n"
            "メール：Test.User+Tag@example.COM に連絡してください。\n"
            "操作：secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about\n"
            "識別子：01234567-89ab-cdef-0123-456789abcdef\n"
            "コード：`if (x < 2) return \"Hello\";`\n"
            "場所：[secondlife://Ahern/128/128/23 東京で会いましょう]\n"
            "また明日。";
        const auto mixedJSON = scannerJSON(mixedJapanese);
        al_native_im_translate_segments(34, mixedJSON.c_str(), "ja", "en", complete);
        check(waitFor(34), "production scanner mixed Japanese native request completes");
        NSDictionary* mixedResult = result(34);
        if ([mixedResult[@"ok"] boolValue])
        {
            const auto output = resultText(mixedResult);
            check(output != mixedJapanese && output.find("こんにちは") == std::string::npos, "production Japanese prose is translated");
            checkProtectedSpans(mixedJapanese, output);
            for (const auto* literal : {"https://example.com/CaSe?x=%2F&name=caf%C3%A9#Anchor", "Test.User+Tag@example.COM",
                                        "secondlife:///app/agent/01234567-89ab-cdef-0123-456789abcdef/about",
                                        "01234567-89ab-cdef-0123-456789abcdef", "`if (x < 2) return \"Hello\";`",
                                        "[secondlife://Ahern/128/128/23 東京で会いましょう]", "🙂"})
            {
                check(output.find(literal) != std::string::npos, "mixed native result retains exact URL email action UUID code SLURL emoji bytes");
            }
        }
        else check(errorIs(mixedResult, @"not_installed") && !requireLive, "--live requires production Japanese mixed translation");
        std::printf("LIVE production mixed ja->en %s\n", results[34].c_str());

        const std::string emote = "/me 微笑んで、こちらを見てください：https://example.com/CaSe?x=%2F\nまた明日。";
        const auto prepared = al::native_im_translation::prepareMessage(emote);
        const auto emoteJSON = scannerJSON(prepared.body);
        al_native_im_translate_segments(35, emoteJSON.c_str(), "ja", "en", complete);
        check(waitFor(35), "production prepared emote native request completes");
        NSDictionary* emoteResult = result(35);
        if ([emoteResult[@"ok"] boolValue])
        {
            const auto translatedBody = resultText(emoteResult);
            const auto assembled = prepared.assemble(translatedBody);
            check(prepared.prefix == "/me " && assembled.rfind("/me ", 0) == 0, "production emote marker remains exact");
            check(assembled != emote, "production emote prose is translated");
            checkProtectedSpans(emote, assembled);
            check(assembled.find("https://example.com/CaSe?x=%2F") != std::string::npos, "production emote URL bytes remain exact");
        }
        else check(errorIs(emoteResult, @"not_installed") && !requireLive, "--live requires production emote translation");
        std::printf("LIVE production emote ja->en %s\n", results[35].c_str());

        // Let requests enter asynchronous SDK work before cancellation. A
        // result already delivered before cancel is valid; no later result is.
        for (uint64_t index = 0; index < 4; ++index)
        {
            const uint64_t request = 40 + index;
            al_native_im_translate(request, "Hello, how are you doing today? I hope we can meet tomorrow and discuss our plans for the coming weekend.", "en", "es", complete);
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.005 + 0.02 * index]];
            const bool alreadyCompleted = results.count(request) != 0;
            al_native_im_translation_cancel(request);
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
            check(alreadyCompleted || !results.count(request), "in-flight cancellation suppresses late callback");
        }

        al_native_im_translate(8, original, "en", "es", complete);
        al_native_im_translation_cancel(8);
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        check(!results.count(8), "translation cancellation suppresses callback");
        al_native_im_translate(9, original, "en", "es", complete);
        al_native_im_translation_shutdown();
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        check(!results.count(9), "shutdown suppresses translation callback");
        al_native_im_translation_languages(10, complete);
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        check(!results.count(10), "shutdown rejects new operations");
        std::printf("Native translation probe: %d checks, %d failures.\n", checks, failures);
        return failures ? 1 : 0;
    }
}
