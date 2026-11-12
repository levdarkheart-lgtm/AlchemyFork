/**
 * @file llstreamingaudio_coreaudio_test.mm
 * @brief Actual native radio lifecycle and output coexistence over loopback HTTP.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project.
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "../llstreamingaudio_coreaudio.h"
#include "../../newview/llviewermedia_streamingaudio.h"
#include <catch2/catch_test_macros.hpp>
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <cstring>
#include <iostream>
#include <thread>
#include <type_traits>

static_assert(std::is_same_v<LLStreamingAudio_MediaPlugins, LLStreamingAudio_CoreAudio>,
              "Both historical startup routes must select the native music backend.");

namespace
{
std::string fixtureBase()
{
    const char* value = std::getenv("AL_NATIVE_MUSIC_TEST_URL");
    if (!value || !*value)
    {
        SKIP("Run tools/metal/tests/native_music_server.py --binary <this binary> for native loopback playback.");
    }
    const std::string base(value);
    REQUIRE(base.rfind("http://127.0.0.1:", 0) == 0);
    return base;
}

void tick(LLStreamingAudio_CoreAudio& player)
{
    @autoreleasepool
    {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        player.update();
    }
}

bool until(LLStreamingAudio_CoreAudio& player, const std::function<bool()>& predicate, double seconds = 10.)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < deadline)
    {
        tick(player);
        if (predicate()) return true;
    }
    return predicate();
}

void requireProgress(LLStreamingAudio_CoreAudio& player, double minimum = .25, double timeout = 10.)
{
    const auto began = std::chrono::steady_clock::now();
    const bool progressed = until(player, [&] { return player.getDiagnostics()["rendered_seconds"].asReal() >= minimum; }, timeout);
    INFO("Native state: " << player.getDiagnostics()["state"].asString());
    INFO("Native error: " << player.getDiagnostics()["error"].asString());
    REQUIRE(progressed);
    const LLSD current = player.getDiagnostics();
    if (current["backend"].asString() == "CoreAudio") CHECK(current["sample_rate"].asReal() > 0.);
    else CHECK(current["backend"].asString() == "AVFoundation");
    CHECK(player.isPlaying() == 1);
    if (std::getenv("AL_NATIVE_MUSIC_TEST_TRACE"))
    {
        const LLSD diagnostics = player.getDiagnostics();
        std::cout << "Native output progression: backend=" << diagnostics["backend"].asString()
                  << " state=" << diagnostics["state"].asString()
                  << " sample_rate=" << diagnostics["sample_rate"].asReal()
                  << " rendered_seconds=" << diagnostics["rendered_seconds"].asReal()
                  << " queued_seconds=" << diagnostics["queued_seconds"].asReal()
                  << " wait_seconds=" << std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count()
                  << '\n';
    }
}

AudioDeviceID defaultDevice(AudioObjectPropertySelector selector)
{
    AudioDeviceID output = kAudioObjectUnknown;
    UInt32 size = sizeof(output);
    AudioObjectPropertyAddress property{selector,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    REQUIRE(AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, nullptr, &size, &output) == noErr);
    return output;
}

AudioDeviceID defaultOutputDevice()
{
    return defaultDevice(kAudioHardwarePropertyDefaultOutputDevice);
}

AudioDeviceID defaultInputDevice()
{
    return defaultDevice(kAudioHardwarePropertyDefaultInputDevice);
}

std::string outputUID(AudioDeviceID device)
{
    CFStringRef uid = nullptr;
    UInt32 size = sizeof(uid);
    AudioObjectPropertyAddress property{kAudioDevicePropertyDeviceUID,
                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    REQUIRE(AudioObjectGetPropertyData(device, &property, 0, nullptr, &size, &uid) == noErr);
    REQUIRE(uid != nullptr);
    const char* utf8 = [(__bridge NSString*)uid UTF8String];
    const std::string result = utf8 ? utf8 : "";
    CFRelease(uid);
    REQUIRE_FALSE(result.empty());
    return result;
}

// A separate, muted native output stands in for a voice playback graph. This
// tests coexistence and ownership, not WebRTC connectivity or microphone input.
struct IndependentOutput
{
    AudioQueueRef queue = nullptr;
    std::atomic<bool> active{true};
    static void refill(void* context, AudioQueueRef output, AudioQueueBufferRef buffer)
    {
        auto& owner = *static_cast<IndependentOutput*>(context);
        if (owner.active.load()) AudioQueueEnqueueBuffer(output, buffer, 0, nullptr);
    }
    IndependentOutput()
    {
        AudioStreamBasicDescription format{};
        format.mSampleRate = 44100;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mBytesPerPacket = format.mBytesPerFrame = sizeof(float) * 2;
        format.mFramesPerPacket = 1;
        format.mChannelsPerFrame = 2;
        format.mBitsPerChannel = sizeof(float) * 8;
        REQUIRE(AudioQueueNewOutput(&format, refill, this, nullptr, nullptr, 0, &queue) == noErr);
        REQUIRE(AudioQueueSetParameter(queue, kAudioQueueParam_Volume, 0.f) == noErr);
        for (int i = 0; i != 3; ++i)
        {
            AudioQueueBufferRef buffer = nullptr;
            REQUIRE(AudioQueueAllocateBuffer(queue, 441 * format.mBytesPerFrame, &buffer) == noErr);
            buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
            std::memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
            REQUIRE(AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr) == noErr);
        }
        REQUIRE(AudioQueueStart(queue, nullptr) == noErr);
    }
    ~IndependentOutput()
    {
        active.store(false);
        if (queue)
        {
            AudioQueueStop(queue, true);
            AudioQueueDispose(queue, true);
        }
    }
    double seconds() const
    {
        AudioTimeStamp timestamp{};
        Boolean discontinuity = false;
        if (AudioQueueGetCurrentTime(queue, nullptr, &timestamp, &discontinuity) != noErr ||
            !(timestamp.mFlags & kAudioTimeStampSampleTimeValid)) return 0.;
        return timestamp.mSampleTime / 44100.;
    }
};
}

TEST_CASE("Native music accepts ordinary parcel streams through Core Audio", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    const char* available = std::getenv("AL_NATIVE_MUSIC_TEST_FORMATS");
    const std::string formats = available ? available : "mp3,aac,heaac";
    for (const std::string format : {"mp3", "aac", "heaac", "vorbis", "opus"})
    {
        if (formats.find(format) == std::string::npos) continue;
        DYNAMIC_SECTION(format)
        {
            LLStreamingAudio_CoreAudio player;
            player.setGain(0.f);
            player.start(base + "/" + format);
            requireProgress(player);
            CHECK(player.getDiagnostics()["backend"].asString() == "CoreAudio");
            if (format == "heaac") CHECK(player.getDiagnostics()["sample_rate"].asReal() == 44100.);
            player.stop();
            CHECK(player.isPlaying() == 0);
            CHECK(player.getURL().empty());
        }
    }
}

TEST_CASE("Native music resolves redirects and playlist URLs", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    for (const std::string path : {"/redirect", "/test.pls", "/test.m3u", "/nested.m3u", "/mime-error",
                                   "/failover404.pls", "/failover500.pls", "/query.m3u"})
    {
        DYNAMIC_SECTION(path)
        {
            LLStreamingAudio_CoreAudio player;
            player.setGain(0.f);
            const std::string original = " \t" + base + path + " legacy parcel label ";
            player.start(original);
            requireProgress(player);
            CHECK(player.getURL() == original);
        }
    }
}

TEST_CASE("Native music plays HLS through the Apple media path", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    const char* available = std::getenv("AL_NATIVE_MUSIC_TEST_FORMATS");
    if (!available || std::string(available).find("hls") == std::string::npos)
    {
        SKIP("HLS fixture requires ffmpeg when running native_music_server.py.");
    }
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    player.start(base + "/radio.m3u8");
    requireProgress(player);
    CHECK(player.getDiagnostics()["backend"].asString() == "AVFoundation");
    player.stop();
    CHECK(player.isPlaying() == 0);
    CHECK(player.getURL().empty());
}

TEST_CASE("Native music delivers split ICY tags on the viewer update thread", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    for (const std::string path : {"/icy", "/icy-latin1", "/legacy"})
    {
        DYNAMIC_SECTION(path)
        {
            LLStreamingAudio_CoreAudio player;
            player.setGain(0.f);
            LLSD observed;
            unsigned callbacks = 0;
            const std::thread::id owner = std::this_thread::get_id();
            auto connection = player.setMetadataUpdatedCallback([&](const LLSD& metadata)
            {
                CHECK(std::this_thread::get_id() == owner);
                observed = metadata;
                ++callbacks;
            });
            player.start(base + path);
            REQUIRE(until(player, [&] { return observed["TITLE"].asString() == (path == "/icy-latin1" ? "Caf\xc3\xa9" : "Probe Song"); }));
            CHECK(observed["ARTIST"].asString() == "Probe Artist");
            const std::string station = observed.has("TRSN") ? observed["TRSN"].asString() : observed["icy-name"].asString();
            CHECK(station == "Native Probe Station");
            CHECK(callbacks > 0);
            requireProgress(player);
            player.stop();
            player.update();
            CHECK(player.getMetadata().size() == 0);
            connection.disconnect();
        }
    }
}

TEST_CASE("Native music pause gain and stop preserve the transport contract", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    CHECK(player.isPlaying() == 0);
    CHECK(player.supportsMetaData());
    CHECK_FALSE(player.supportsWaveData());
    CHECK_FALSE(player.supportsAdjustableBufferSizes());
    player.setGain(-.5f); CHECK(player.getGain() == 0.f);
    player.setGain(1.5f); CHECK(player.getGain() == 1.f);
    player.setGain(std::numeric_limits<float>::quiet_NaN()); CHECK(std::isfinite(player.getGain()));
    player.setGain(0.f);
    player.start(base + "/mp3");
    REQUIRE(player.isPlaying() == 1); // Pending requests remain cancelable.
    requireProgress(player);
    player.pause(1);
    CHECK(player.isPlaying() == 2);
    const double paused = player.getDiagnostics()["rendered_seconds"].asReal();
    until(player, [] { return false; }, .2);
    CHECK(player.getDiagnostics()["rendered_seconds"].asReal() <= paused + .1);
    CHECK(player.getURL() == base + "/mp3");
    player.pause(0);
    requireProgress(player, paused + .1);
    player.pause(-1); CHECK(player.isPlaying() == 2);
    player.pause(-1); CHECK(player.isPlaying() == 1);
    player.start("");
    CHECK(player.isPlaying() == 0);
    CHECK(player.getURL().empty());
}

TEST_CASE("Native music cancellation rejects callbacks from replaced requests", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    player.start(base + "/hang");
    CHECK(player.isPlaying() == 1);
    until(player, [] { return false; }, .05);
    player.stop();
    CHECK(player.isPlaying() == 0);
    player.start(base + "/icy");
    REQUIRE(until(player, [&] { return player.getMetadata()["TITLE"].asString() == "Probe Song"; }));
    player.start(base + "/aac");
    requireProgress(player);
    CHECK(player.getMetadata()["TITLE"].asString().empty());
    CHECK(player.getURL() == base + "/aac");
    player.stop();
    until(player, [] { return false; }, .1);
    CHECK(player.isPlaying() == 0);
}

TEST_CASE("Native music recovers from a disconnected source", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    player.start(base + "/disconnect?test=" + std::to_string(unique));
    requireProgress(player);
    REQUIRE(until(player, [&] { return player.getDiagnostics()["retry_count"].asInteger() > 0; }, 12.));
    const double before_reconnection = player.getDiagnostics()["rendered_seconds"].asReal();
    REQUIRE(until(player, [&] { return player.getDiagnostics()["state"].asString() == "playing" &&
                                      player.getDiagnostics()["rendered_seconds"].asReal() > before_reconnection + .25; }, 12.));
    CHECK(player.isPlaying() == 1);
    player.stop();
    until(player, [] { return false; }, .2);
    CHECK(player.isPlaying() == 0);
}

TEST_CASE("Native music drains finite output until the actual sample clock reaches its tail", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    const auto began = std::chrono::steady_clock::now();
    player.start(base + "/finite.mp3");
    requireProgress(player);
    // This file is delivered faster than playback. AudioQueue can return
    // compressed input buffers before device samples finish; those callbacks
    // must not retire the remaining output or report finite playback complete.
    REQUIRE(until(player, [&] { return player.getDiagnostics()["rendered_seconds"].asReal() >= 3.9 ||
                                      player.isPlaying() == 0; }, 8.));
    const double played = player.getDiagnostics()["rendered_seconds"].asReal();
    CHECK(played >= 3.9);
    CHECK(std::chrono::steady_clock::now() - began >= std::chrono::milliseconds(3500));
    const bool completed = until(player, [&] { return player.isPlaying() == 0; }, 4.);
    const LLSD tail = player.getDiagnostics();
    INFO("Finite tail state=" << tail["state"].asString()
         << " rendered_seconds=" << tail["rendered_seconds"].asReal()
         << " queued_seconds=" << tail["queued_seconds"].asReal()
         << " error=" << tail["error"].asString());
    REQUIRE(completed);
    CHECK(player.getDiagnostics()["state"].asString() == "stopped");
    CHECK(player.getDiagnostics()["rendered_seconds"].asReal() >= 3.9);
    CHECK(player.getDiagnostics()["error"].asString().empty());
    player.stop();
}

TEST_CASE("Native music resumes an underflow on the same connection", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    player.start(base + "/stall");
    requireProgress(player);
    // The server pauses after two seconds of offered data for one second,
    // keeping the connection open. This exhausts the small startup prebuffer.
    REQUIRE(until(player, [&] { const LLSD diagnostics = player.getDiagnostics();
                               return diagnostics["state"].asString() == "buffering" &&
                                      diagnostics["rendered_seconds"].asReal() >= 1.; }, 8.));
    const double gap = player.getDiagnostics()["rendered_seconds"].asReal();
    REQUIRE(until(player, [&] { const LLSD diagnostics = player.getDiagnostics();
                               return diagnostics["state"].asString() == "playing" &&
                                      diagnostics["rendered_seconds"].asReal() > std::max(2.2, gap + .25); }, 8.));
    CHECK(player.getDiagnostics()["retry_count"].asInteger() == 0);
    player.stop();
    CHECK(player.isPlaying() == 0);
}

TEST_CASE("Native music rejects invalid or recursive playlists within bounds", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    for (const std::string& url : {std::string("file:///etc/hosts"), std::string("not a URL"),
                                  base + "/cycle.m3u", base + "/invalid.pls", base + "/large.m3u"})
    {
        DYNAMIC_SECTION(url)
        {
            LLStreamingAudio_CoreAudio player;
            player.setGain(0.f);
            player.start(url);
            REQUIRE(until(player, [&] { return player.isPlaying() == 0; }, 6.));
            CHECK_FALSE(player.getDiagnostics()["error"].asString().empty());
            CHECK(player.getDiagnostics()["queued_seconds"].asReal() <= 1.);
            player.stop();
        }
    }
}

TEST_CASE("Native music leaves independent output and system routing intact", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    const AudioDeviceID before = defaultOutputDevice();
    const AudioDeviceID input_before = defaultInputDevice();
    IndependentOutput separate;
    {
        LLStreamingAudio_CoreAudio player;
        player.setGain(0.f);
        player.start(base + "/mp3");
        requireProgress(player);
        const double independent_before = separate.seconds();
        player.stop();
        player.start(base + "/aac");
        requireProgress(player);
        CHECK(separate.seconds() > independent_before);
        CHECK(defaultOutputDevice() == before);
        CHECK(defaultInputDevice() == input_before);
    }
    CHECK(defaultOutputDevice() == before);
    CHECK(defaultInputDevice() == input_before);
}

TEST_CASE("Native music output selection and fallback leave system devices unchanged", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    const AudioDeviceID output_before = defaultOutputDevice();
    const AudioDeviceID input_before = defaultInputDevice();
    const std::string current_uid = outputUID(output_before);
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    player.start(base + "/mp3");
    requireProgress(player);
    for (const std::string& uid : {current_uid, std::string(), std::string("native-music-test-nonexistent-device")})
    {
        const double before = player.getDiagnostics()["rendered_seconds"].asReal();
        player.setOutputDevice(uid);
        requireProgress(player, before + .1);
        CHECK(defaultOutputDevice() == output_before);
        CHECK(defaultInputDevice() == input_before);
    }
    player.stop();
    CHECK(player.isPlaying() == 0);
}

TEST_CASE("Native music terminates playlists whose alternates replenish a cycle", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    LLStreamingAudio_CoreAudio player;
    player.setGain(0.f);
    player.start(base + "/cycle-a.pls");
    REQUIRE(until(player, [&] { return player.isPlaying() == 0; }, 15.));
    CHECK(player.getDiagnostics()["state"].asString() == "failed");
    CHECK_FALSE(player.getDiagnostics()["error"].asString().empty());
    CHECK(player.getDiagnostics()["queued_seconds"].asReal() <= 1.);
    player.stop();
}

TEST_CASE("Native music destruction cancels pending and active native callbacks", "[native_music][coreaudio]")
{
    const std::string base = fixtureBase();
    for (const std::string path : {"/hang", "/mp3"})
    {
        DYNAMIC_SECTION(path)
        {
            auto player = std::make_unique<LLStreamingAudio_CoreAudio>();
            player->setGain(0.f);
            player->start(base + path);
            if (path == "/mp3") requireProgress(*player);
            else until(*player, [] { return false; }, .05);
            const auto began = std::chrono::steady_clock::now();
            player.reset();
            CHECK(std::chrono::steady_clock::now() - began < std::chrono::seconds(2));
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];
        }
    }
}
