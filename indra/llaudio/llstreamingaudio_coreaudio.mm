/**
 * @file llstreamingaudio_coreaudio.mm
 * @brief Apple-native parcel radio. No microphone, voice engine or global audio session is used.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License, version 2.1 only.
 */

#include "linden_common.h"
#include "llstreamingaudio_coreaudio.h"
#include "llsdutil.h"

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <vector>

namespace
{
constexpr std::size_t QUEUE_BUFFERS = 16;
constexpr UInt32 BUFFER_BYTES = 65536;
constexpr UInt32 BUFFER_PACKETS = 512;
constexpr std::size_t MAX_PENDING_BYTES = 4 * 1024 * 1024;
constexpr std::size_t MAX_PENDING_PACKETS = 16384;
constexpr std::size_t MAX_PLAYLIST_BYTES = 512 * 1024;
constexpr std::size_t MAX_HEADER_BYTES = 65536;
constexpr double START_BUFFER_SECONDS = 0.5;
constexpr double OUTPUT_BUFFER_SECONDS = 0.1;
constexpr double HIGH_WATER_SECONDS = 6.0;
constexpr double LOW_WATER_SECONDS = 2.0;
constexpr unsigned MAX_RETRIES = 6;
constexpr unsigned MAX_REQUESTS_WITHOUT_STABLE_PLAYBACK = 32;
char sQueueIdentity;

struct CompressedPacket
{
    std::vector<std::uint8_t> bytes;
    UInt32 frames = 0;
};

struct QueueSlot
{
    AudioQueueBufferRef buffer = nullptr;
    std::atomic<bool> available{true};
    std::uint64_t frames = 0;
};

// Only these atomics are touched by the Core Audio output thread. No allocation,
// logging, Objective-C dispatch or network work occurs on that callback.
struct OutputState
{
    std::array<QueueSlot, QUEUE_BUFFERS> slots;
    std::atomic<std::uint64_t> queuedFrames{0};
    std::atomic<bool> hasRun{false};
};

void outputBufferFinished(void* context, AudioQueueRef, AudioQueueBufferRef buffer)
{
    auto* state = static_cast<OutputState*>(context);
    auto* slot = static_cast<QueueSlot*>(buffer->mUserData);
    state->queuedFrames.fetch_sub(slot->frames, std::memory_order_relaxed);
    slot->available.store(true, std::memory_order_release);
}

void outputRunningChanged(void* context, AudioQueueRef queue, AudioQueuePropertyID)
{
    UInt32 running = 0;
    UInt32 size = sizeof(running);
    if (AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &running, &size) == noErr && running)
        static_cast<OutputState*>(context)->hasRun.store(true, std::memory_order_release);
}

NSString* decodedText(NSData* data)
{
    if (!data.length) return @"";
    NSString* value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!value) value = [[NSString alloc] initWithData:data encoding:NSWindowsCP1252StringEncoding];
    if (!value) value = [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    return value ?: @"";
}

NSString* stringFromUTF8(const std::string& value)
{
    return [[NSString alloc] initWithBytes:value.data() length:value.size()
                                encoding:NSUTF8StringEncoding];
}

bool isHTTPURL(NSURL* url)
{
    NSString* scheme = url.scheme.lowercaseString;
    return url.host.length && ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]);
}

bool hasOutputDevice(NSString* uid)
{
    if (!uid.length) return false;
    CFStringRef identifier = (__bridge CFStringRef)uid;
    AudioDeviceID device = kAudioObjectUnknown;
    AudioValueTranslation translation{&identifier, sizeof(identifier), &device, sizeof(device)};
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDeviceForUID,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 size = sizeof(translation);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &translation) != noErr ||
        device == kAudioObjectUnknown) return false;
    address = {kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};
    UInt32 streams = 0;
    return AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &streams) == noErr && streams > 0;
}

NSURL* cleanedURL(NSString* original)
{
    if (!original) return nil;
    NSString* text = [original stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    // Historic parcel URLs may append an unescaped label after an ASCII space.
    NSRange label = [text rangeOfString:@" "];
    if (label.location != NSNotFound) text = [text substringToIndex:label.location];
    if (text.length > 16384) return nil;
    NSURL* url = [NSURL URLWithString:text];
    return isHTTPURL(url) ? url : nil;
}

LLSD dictionaryToLLSD(NSDictionary* dictionary)
{
    LLSD result = LLSD::emptyMap();
    for (NSString* key in dictionary)
    {
        if (![key isKindOfClass:NSString.class]) continue;
        id value = dictionary[key];
        const std::string name(key.UTF8String ?: "");
        if ([value isKindOfClass:NSString.class])
            result[name] = std::string([value UTF8String] ?: "");
        else if ([value isKindOfClass:NSNumber.class])
            result[name] = [value doubleValue];
    }
    return result;
}
}

@interface LLNativeMusic : NSObject <NSURLSessionDataDelegate, AVPlayerItemMetadataOutputPushDelegate>
{
    dispatch_queue_t _worker;
    dispatch_source_t _timer;
    NSOperationQueue* _delegateQueue;
    NSURLSession* _session;
    NSURLSessionDataTask* _task;
    NSURL* _resolvedURL;
    NSURL* _requestURL;
    NSMutableSet<NSString*>* _visited;
    NSMutableArray<NSURL*>* _alternates;
    unsigned _redirects;
    unsigned _playlistDepth;
    unsigned _retries;
    unsigned _attempts;
    std::uint64_t _generation;
    bool _closed;
    bool _requested;
    bool _paused;
    bool _taskSuspended;
    bool _finite;
    bool _endOfInput;
    bool _queueStarted;
    bool _queueDraining;
    bool _queueEverStarted;
    bool _queueDeviceAssigned;
    bool _preferPlayer;
    bool _isPlaylist;
    bool _bodyStarted;
    double _retryAt;
    double _lastDataAt;
    double _requestAt;
    double _firstOutputAt;
    double _renderedSeconds;
    double _totalFrames;
    double _clockOffsetFrames;
    double _lastClockSample;
    double _pendingFrames;
    double _nominalBitRate;
    float _gain;
    NSString* _outputUID;
    NSString* _state;
    NSString* _error;
    NSMutableDictionary<NSString*, NSString*>* _metadata;
    NSMutableData* _prefix;
    NSMutableData* _playlist;
    NSMutableData* _icyMetadata;
    NSUInteger _icyInterval;
    NSUInteger _icyAudioRemaining;
    NSUInteger _icyMetadataRemaining;
    bool _icyNeedsLength;
    AudioFileTypeID _typeHint;
    AudioFileStreamID _parser;
    AudioQueueRef _queue;
    AudioQueueTimelineRef _timeline;
    AudioStreamBasicDescription _format;
    std::unique_ptr<OutputState> _output;
    std::deque<CompressedPacket> _packets;
    std::size_t _packetBytes;
    OSStatus _parserFailure;
    AVPlayer* _player;
    AVPlayerItemMetadataOutput* _playerMetadata;
    std::mutex _snapshotMutex;
    NSDictionary* _snapshotMetadata;
    NSDictionary* _snapshotDiagnostics;
    std::atomic<int> _snapshotPlaying;
}
- (instancetype)initWithOutputUID:(NSString*)uid;
- (void)sync:(dispatch_block_t)block;
- (void)close;
- (void)startURL:(NSURL*)url;
- (void)stop;
- (void)setPaused:(BOOL)paused;
- (void)setGain:(float)gain;
- (void)setOutputUID:(NSString*)uid;
- (NSDictionary*)metadata;
- (NSDictionary*)diagnostics;
- (int)playing;
- (void)streamProperty:(AudioFileStreamPropertyID)property;
- (void)streamPackets:(UInt32)bytes count:(UInt32)count data:(const void*)data
        descriptions:(AudioStreamPacketDescription*)descriptions;
@end

namespace
{
void streamPropertyChanged(void* context, AudioFileStreamID, AudioFileStreamPropertyID property,
                           AudioFileStreamPropertyFlags*)
{
    [(__bridge LLNativeMusic*)context streamProperty:property];
}

void streamPacketsReady(void* context, UInt32 bytes, UInt32 count, const void* data,
                        AudioStreamPacketDescription* descriptions)
{
    [(__bridge LLNativeMusic*)context streamPackets:bytes count:count data:data descriptions:descriptions];
}
}

@implementation LLNativeMusic

- (instancetype)initWithOutputUID:(NSString*)uid
{
    if ((self = [super init]))
    {
        dispatch_queue_attr_t attributes = dispatch_queue_attr_make_with_autorelease_frequency(
            DISPATCH_QUEUE_SERIAL, DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM);
        _worker = dispatch_queue_create("org.alchemyviewer.parcel-music", attributes);
        dispatch_queue_set_specific(_worker, &sQueueIdentity, (__bridge void*)self, nullptr);
        _delegateQueue = [[NSOperationQueue alloc] init];
        _delegateQueue.maxConcurrentOperationCount = 1;
        _delegateQueue.underlyingQueue = _worker;
        NSURLSessionConfiguration* configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
        configuration.URLCache = nil;
        configuration.HTTPCookieStorage = nil;
        configuration.HTTPShouldSetCookies = NO;
        configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        configuration.timeoutIntervalForRequest = 20;
        configuration.timeoutIntervalForResource = 7 * 24 * 60 * 60;
        _session = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:_delegateQueue];
        _gain = 1.f;
        _outputUID = [uid copy] ?: @"";
        _state = @"stopped";
        _error = @"";
        _metadata = [NSMutableDictionary dictionary];
        [self publish];
    }
    return self;
}

- (void)sync:(dispatch_block_t)block
{
    if (dispatch_get_specific(&sQueueIdentity) == (__bridge void*)self) block();
    else dispatch_sync(_worker, block);
}

- (double)now { return NSProcessInfo.processInfo.systemUptime; }

- (void)ensureTimer
{
    if (_timer || _closed) return;
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _worker);
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC),
                              50 * NSEC_PER_MSEC, 10 * NSEC_PER_MSEC);
    __weak LLNativeMusic* weakSelf = self;
    dispatch_source_set_event_handler(_timer, ^{ [weakSelf tick]; });
    dispatch_resume(_timer);
}

- (void)cancelTimer
{
    if (_timer) dispatch_source_cancel(_timer);
    _timer = nil;
}

- (void)disposePlayback
{
    if (_task)
    {
        [_task cancel];
        _task = nil;
    }
    _taskSuspended = false;
    if (_player)
    {
        [_player pause];
        if (_playerMetadata) [_player.currentItem removeOutput:_playerMetadata];
        [_player replaceCurrentItemWithPlayerItem:nil];
        _playerMetadata = nil;
        _player = nil;
    }
    if (_queue)
    {
        // Synchronous disposal completes every output callback before its
        // OutputState storage is released. Never hold a callback lock here.
        if (_timeline) AudioQueueDisposeTimeline(_queue, _timeline);
        _timeline = nullptr;
        AudioQueueRemovePropertyListener(_queue, kAudioQueueProperty_IsRunning,
            outputRunningChanged, _output.get());
        AudioQueueDispose(_queue, true);
        _queue = nullptr;
    }
    _output.reset();
    if (_parser)
    {
        AudioFileStreamClose(_parser);
        _parser = nullptr;
    }
    _packets.clear();
    _packetBytes = 0;
    _pendingFrames = 0;
    _totalFrames = 0;
    _clockOffsetFrames = 0;
    _lastClockSample = 0;
    _renderedSeconds = 0;
    _firstOutputAt = 0;
    _format = {};
    _queueStarted = false;
    _queueDraining = false;
    _queueEverStarted = false;
    _queueDeviceAssigned = false;
    _endOfInput = false;
    _parserFailure = noErr;
    _icyInterval = _icyAudioRemaining = _icyMetadataRemaining = 0;
    _icyNeedsLength = false;
    _icyMetadata = [NSMutableData data];
    _prefix = [NSMutableData data];
    _playlist = [NSMutableData data];
    _bodyStarted = false;
    _isPlaylist = false;
    _finite = false;
    _typeHint = 0;
    _nominalBitRate = 0;
}

- (void)close
{
    [self sync:^{
        if (self->_closed) return;
        self->_closed = true;
        self->_requested = false;
        [self disposePlayback];
        [self cancelTimer];
        [self->_session invalidateAndCancel];
        self->_session = nil;
    }];
}

- (void)stop
{
    [self sync:^{
        ++self->_generation;
        self->_requested = self->_paused = false;
        self->_retryAt = 0;
        [self cancelTimer];
        [self disposePlayback];
        self->_resolvedURL = nil;
        self->_requestURL = nil;
        self->_state = @"stopped";
        self->_error = @"";
        self->_renderedSeconds = 0;
        self->_firstOutputAt = 0;
        [self->_metadata removeAllObjects];
        [self publish];
    }];
}

- (void)startURL:(NSURL*)url
{
    [self sync:^{
        if (self->_closed) return;
        ++self->_generation;
        [self disposePlayback];
        self->_requested = url != nil;
        self->_paused = false;
        self->_retries = 0;
        self->_attempts = 0;
        self->_retryAt = 0;
        self->_playlistDepth = 0;
        self->_preferPlayer = false;
        self->_renderedSeconds = 0;
        self->_firstOutputAt = 0;
        self->_error = url ? @"" : @"invalid_http_url";
        self->_state = url ? @"connecting" : @"failed";
        self->_resolvedURL = url;
        self->_visited = [NSMutableSet set];
        self->_alternates = [NSMutableArray array];
        [self->_metadata removeAllObjects];
        if (url)
        {
            [self ensureTimer];
            [self requestURL:url];
        }
        else [self cancelTimer];
        [self publish];
    }];
}

- (void)setPaused:(BOOL)paused
{
    [self sync:^{
        if (!self->_resolvedURL || self->_closed ||
            (self->_paused == static_cast<bool>(paused) && self->_requested)) return;
        ++self->_generation;
        self->_paused = paused;
        self->_requested = true;
        self->_retryAt = 0;
        [self disposePlayback];
        if (paused)
        {
            self->_state = @"paused";
            [self cancelTimer];
        }
        else
        {
            self->_retries = 0;
            self->_attempts = 0;
            self->_playlistDepth = 0;
            self->_visited = [NSMutableSet set];
            self->_renderedSeconds = 0;
            self->_firstOutputAt = 0;
            self->_error = @"";
            [self ensureTimer];
            [self requestURL:self->_resolvedURL];
        }
        [self publish];
    }];
}

- (void)setGain:(float)gain
{
    // The viewer updates gain during each fade frame. Keep that path off the
    // render thread; the serial queue preserves gain-before-start ordering.
    dispatch_async(_worker, ^{
        if (self->_closed) return;
        self->_gain = gain;
        if (self->_queue) AudioQueueSetParameter(self->_queue, kAudioQueueParam_Volume, gain);
        if (self->_player) self->_player.volume = gain;
        [self publish];
    });
}

- (void)applyOutputUID
{
    const bool selected = _outputUID.length && hasOutputDevice(_outputUID);
    if (_outputUID.length && !selected) _error = @"output_device_unavailable";
    if (_queue)
    {
        // A freshly created queue already follows the default output. Passing
        // a NULL CFString is accepted by SetProperty but makes Start fail with
        // InvalidDevice. The AudioQueue default endpoint is a distinct UID.
        if (!selected && !_queueDeviceAssigned) return;
        CFStringRef device = selected ? (__bridge CFStringRef)_outputUID : CFSTR("AQDefaultDevice");
        OSStatus status = AudioQueueSetProperty(_queue, kAudioQueueProperty_CurrentDevice, &device, sizeof(device));
        _queueDeviceAssigned = true;
        if (status != noErr)
        {
            _error = [NSString stringWithFormat:@"output_device:%d", static_cast<int>(status)];
            // An unavailable selected endpoint must not mutate the system
            // default or a voice endpoint. Follow the system output instead.
            device = CFSTR("AQDefaultDevice");
            AudioQueueSetProperty(_queue, kAudioQueueProperty_CurrentDevice, &device, sizeof(device));
        }
    }
    if (_player) _player.audioOutputDeviceUniqueID = selected ? _outputUID : nil;
}

- (void)setOutputUID:(NSString*)uid
{
    [self sync:^{
        self->_outputUID = [uid copy] ?: @"";
        [self applyOutputUID];
        [self publish];
    }];
}

- (void)requestURL:(NSURL*)url
{
    if (!_requested || _paused || _closed) return;
    if (++_attempts > MAX_REQUESTS_WITHOUT_STABLE_PLAYBACK)
    {
        // Nested redundant playlists must not replenish alternatives forever.
        // Only a new owner request or proven stable playback resets this cap.
        [_alternates removeAllObjects];
        [self fail:@"request_limit" retry:NO];
        return;
    }
    if (!isHTTPURL(url)) { [self fail:@"invalid_http_url" retry:NO]; return; }
    _resolvedURL = url;
    _requestURL = url;
    _state = _retries ? @"reconnecting" : @"connecting";
    _requestAt = _lastDataAt = [self now];
    _redirects = 0;
    if (_preferPlayer) { [self useNativePlayer]; return; }
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:20];
    [request setValue:@"1" forHTTPHeaderField:@"Icy-MetaData"];
    [request setValue:@"Alchemy Native Audio" forHTTPHeaderField:@"User-Agent"];
    _task = [_session dataTaskWithRequest:request];
    [_task resume];
}

- (void)fail:(NSString*)stage retry:(BOOL)retry
{
    _error = [stage copy];
    [self disposePlayback];
    if (!_requested || _paused || _closed) return;
    const bool alternate = _alternates.count > 0;
    if (alternate)
    {
        _resolvedURL = _alternates.firstObject;
        [_alternates removeObjectAtIndex:0];
        _preferPlayer = false;
        retry = YES;
    }
    if (retry && (alternate || _retries < MAX_RETRIES))
    {
        const double delay = alternate ? 0.25 : std::min(16.0, 0.5 * std::pow(2.0, _retries));
        ++_retries;
        _retryAt = [self now] + delay;
        _state = @"reconnecting";
    }
    else
    {
        _requested = false;
        _retryAt = 0;
        _state = @"failed";
        [self cancelTimer];
    }
    [self publish];
}

- (void)setSongTitle:(NSString*)text
{
    NSString* value = [text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSRange separator = [value rangeOfString:@" - "];
    if (separator.location != NSNotFound)
    {
        _metadata[@"ARTIST"] = [[value substringToIndex:separator.location]
            stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        _metadata[@"TITLE"] = [[value substringFromIndex:NSMaxRange(separator)]
            stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    }
    else
    {
        [_metadata removeObjectForKey:@"ARTIST"];
        _metadata[@"TITLE"] = value ?: @"";
    }
}

- (void)readICYMetadata
{
    const auto* bytes = static_cast<const std::uint8_t*>(_icyMetadata.bytes);
    NSUInteger length = _icyMetadata.length;
    while (length && bytes[length - 1] == 0) --length;
    NSString* text = decodedText([NSData dataWithBytes:bytes length:length]);
    NSRegularExpression* fields = [NSRegularExpression regularExpressionWithPattern:
        @"(?:^|;)\\s*([A-Za-z][A-Za-z0-9_-]*)\\s*=\\s*(['\"])(.*?)\\2\\s*(?=;|$)"
        options:NSRegularExpressionDotMatchesLineSeparators error:nil];
    for (NSTextCheckingResult* match in [fields matchesInString:text options:0 range:NSMakeRange(0, text.length)])
    {
        NSString* key = [[text substringWithRange:[match rangeAtIndex:1]] lowercaseString];
        NSString* value = [text substringWithRange:[match rangeAtIndex:3]];
        if ([key isEqualToString:@"streamtitle"]) [self setSongTitle:value];
        else if ([key isEqualToString:@"streamurl"]) _metadata[@"URL"] = value;
    }
}

- (void)applyHeaders:(NSDictionary*)headers mime:(NSString*)mime expected:(int64_t)length
{
    NSMutableDictionary* lower = [NSMutableDictionary dictionary];
    for (id key in headers)
    {
        if ([key isKindOfClass:NSString.class]) lower[[key lowercaseString]] = [headers[key] description];
    }
    NSString* type = [[mime componentsSeparatedByString:@";"] firstObject].lowercaseString ?: @"";
    NSString* extension = _resolvedURL.pathExtension.lowercaseString;
    _isPlaylist = [type containsString:@"mpegurl"] || [type containsString:@"scpls"] ||
        [type containsString:@"playlist"] || [extension isEqualToString:@"pls"] ||
        [extension isEqualToString:@"m3u"] || [extension isEqualToString:@"m3u8"];
    if ([type containsString:@"aac"]) _typeHint = kAudioFileAAC_ADTSType;
    else if ([type isEqualToString:@"audio/mpeg"]) _typeHint = kAudioFileMP3Type;
    _finite = length >= 0;
    const long long interval = [lower[@"icy-metaint"] longLongValue];
    if (interval > 0 && interval <= 16 * 1024 * 1024)
    {
        _icyInterval = static_cast<NSUInteger>(interval);
        _icyAudioRemaining = _icyInterval;
        _finite = false;
    }
    if (lower[@"icy-br"]) _nominalBitRate = [lower[@"icy-br"] doubleValue] * 1000.0;
    if (lower[@"icy-name"]) _metadata[@"icy-name"] = lower[@"icy-name"];
    if (lower[@"icy-url"]) _metadata[@"icy-url"] = lower[@"icy-url"];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response newRequest:(NSURLRequest*)request
    completionHandler:(void (^)(NSURLRequest*))completion
{
    if (task != _task || _closed || ++_redirects > 12 || !isHTTPURL(request.URL))
    {
        completion(nil);
        if (task == _task) [self fail:@"redirect_limit_or_scheme" retry:NO];
        return;
    }
    _resolvedURL = request.URL;
    completion(request);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)task
    didReceiveResponse:(NSURLResponse*)response
    completionHandler:(void (^)(NSURLSessionResponseDisposition))completion
{
    if (task != _task || _closed) { completion(NSURLSessionResponseCancel); return; }
    if (response.URL) _resolvedURL = response.URL;
    NSHTTPURLResponse* http = [response isKindOfClass:NSHTTPURLResponse.class] ? (NSHTTPURLResponse*)response : nil;
    if (http && (http.statusCode < 200 || http.statusCode >= 300))
    {
        completion(NSURLSessionResponseCancel);
        [self fail:[NSString stringWithFormat:@"http:%ld", static_cast<long>(http.statusCode)]
             retry:http.statusCode >= 500 || http.statusCode == 408 || http.statusCode == 429];
        return;
    }
    [self applyHeaders:http.allHeaderFields ?: @{} mime:response.MIMEType expected:response.expectedContentLength];
    _lastDataAt = [self now];
    _state = @"buffering";
    completion(NSURLSessionResponseAllow);
}

- (void)consumePrefix
{
    if (_bodyStarted || _prefix.length < 8) return;
    const char* bytes = static_cast<const char*>(_prefix.bytes);
    if (_prefix.length >= 4 && std::memcmp(bytes, "ICY ", 4) == 0)
    {
        NSData* terminator = [@"\r\n\r\n" dataUsingEncoding:NSASCIIStringEncoding];
        NSRange end = [_prefix rangeOfData:terminator options:0 range:NSMakeRange(0, _prefix.length)];
        if (end.location == NSNotFound)
        {
            if (_prefix.length > MAX_HEADER_BYTES) [self fail:@"legacy_header_limit" retry:NO];
            return;
        }
        NSString* header = decodedText([_prefix subdataWithRange:NSMakeRange(0, end.location)]);
        NSArray<NSString*>* lines = [header componentsSeparatedByString:@"\r\n"];
        if (![lines.firstObject hasPrefix:@"ICY 200 "])
        {
            [self fail:@"legacy_status" retry:NO];
            return;
        }
        NSMutableDictionary* headers = [NSMutableDictionary dictionary];
        for (NSUInteger index = 1; index < lines.count; ++index)
        {
            NSRange colon = [lines[index] rangeOfString:@":"];
            if (colon.location == NSNotFound) continue;
            headers[[[lines[index] substringToIndex:colon.location] lowercaseString]] =
                [[lines[index] substringFromIndex:NSMaxRange(colon)]
                    stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        }
        [self applyHeaders:headers mime:headers[@"content-type"] ?: @"audio/mpeg" expected:-1];
        [_prefix replaceBytesInRange:NSMakeRange(0, NSMaxRange(end)) withBytes:nullptr length:0];
    }
    else if (std::memcmp(bytes, "#EXTM3U", 7) == 0 || strncasecmp(bytes, "[playlis", 8) == 0)
        _isPlaylist = true;
    _bodyStarted = true;
    NSData* body = _prefix;
    _prefix = [NSMutableData data];
    [self consumeBody:body];
}

- (void)feedAudio:(const std::uint8_t*)bytes length:(NSUInteger)length
{
    if (!length || _parserFailure || _player) return;
    if (!_parser)
    {
        _parserFailure = AudioFileStreamOpen((__bridge void*)self, streamPropertyChanged,
                                             streamPacketsReady, _typeHint, &_parser);
        if (_parserFailure) return;
    }
    while (length && !_parserFailure)
    {
        const UInt32 chunk = static_cast<UInt32>(std::min<NSUInteger>(length, 16384));
        const OSStatus parsed = AudioFileStreamParseBytes(_parser, chunk, bytes, 0);
        // Property/packet callbacks can reject a malformed format or queue
        // allocation even when the parser's own call returns success.
        if (parsed != noErr) _parserFailure = parsed;
        bytes += chunk;
        length -= chunk;
    }
}

- (void)consumeBody:(NSData*)data
{
    if (_isPlaylist)
    {
        if (_playlist.length + data.length > MAX_PLAYLIST_BYTES)
        {
            [self fail:@"playlist_size_limit" retry:NO];
            return;
        }
        [_playlist appendData:data];
        return;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
    NSUInteger remaining = data.length;
    while (remaining && !_parserFailure)
    {
        if (!_icyInterval)
        {
            [self feedAudio:bytes length:remaining];
            break;
        }
        if (_icyAudioRemaining)
        {
            NSUInteger count = std::min(remaining, _icyAudioRemaining);
            [self feedAudio:bytes length:count];
            bytes += count;
            remaining -= count;
            _icyAudioRemaining -= count;
            if (!_icyAudioRemaining) _icyNeedsLength = true;
        }
        else if (_icyNeedsLength)
        {
            _icyMetadataRemaining = static_cast<NSUInteger>(*bytes++) * 16;
            --remaining;
            _icyNeedsLength = false;
            [_icyMetadata setLength:0];
            if (!_icyMetadataRemaining) _icyAudioRemaining = _icyInterval;
        }
        else
        {
            const NSUInteger count = std::min(remaining, _icyMetadataRemaining);
            [_icyMetadata appendBytes:bytes length:count];
            bytes += count;
            remaining -= count;
            _icyMetadataRemaining -= count;
            if (!_icyMetadataRemaining)
            {
                [self readICYMetadata];
                _icyAudioRemaining = _icyInterval;
            }
        }
    }
    if (_parserFailure)
    {
        // Preserve natively supported formats that the packet parser cannot
        // identify, using AVFoundation rather than a VLC/foreign-codec path.
        if (!_queueStarted) [self useNativePlayer];
        else [self fail:[NSString stringWithFormat:@"parse:%d", static_cast<int>(_parserFailure)] retry:YES];
        return;
    }
    [self drainPackets:NO];
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)task didReceiveData:(NSData*)data
{
    if (task != _task || _closed || !_requested || _paused || _player) return;
    _lastDataAt = [self now];
    if (!_bodyStarted)
    {
        const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
        NSUInteger offset = 0;
        while (!_bodyStarted && offset < data.length && _task == task)
        {
            const NSUInteger capacity = _prefix.length < 8 ? 8 - _prefix.length :
                MAX_HEADER_BYTES + 4 - _prefix.length;
            if (!capacity)
            {
                [self fail:@"response_prefix_limit" retry:NO];
                return;
            }
            const NSUInteger take = std::min(capacity, data.length - offset);
            [_prefix appendBytes:bytes + offset length:take];
            offset += take;
            [self consumePrefix];
        }
        if (_bodyStarted && offset < data.length && _task == task)
        {
            // The NSURLSession-owned input remains alive during this
            // synchronous consumption; do not copy a large callback merely to
            // inspect its first few bytes.
            NSData* remainder = [NSData dataWithBytesNoCopy:const_cast<std::uint8_t*>(bytes + offset)
                length:data.length - offset freeWhenDone:NO];
            [self consumeBody:remainder];
        }
    }
    else [self consumeBody:data];
}

- (void)resolvePlaylist
{
    NSString* text = decodedText(_playlist);
    if ([text containsString:@"#EXT-X-"])
    {
        [self useNativePlayer];
        return;
    }
    if (++_playlistDepth > 6)
    {
        [self fail:@"playlist_depth_limit" retry:NO];
        return;
    }
    NSMutableArray<NSURL*>* choices = [NSMutableArray array];
    const bool pls = [text rangeOfString:@"[playlist]" options:NSCaseInsensitiveSearch].location != NSNotFound;
    for (NSString* raw in [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet])
    {
        NSString* line = [raw stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        if (!line.length || [line hasPrefix:@"#"] || [line hasPrefix:@"["]) continue;
        if (pls)
        {
            if (![line.lowercaseString hasPrefix:@"file"]) continue;
            NSRange equal = [line rangeOfString:@"="];
            if (equal.location == NSNotFound) continue;
            line = [[line substringFromIndex:NSMaxRange(equal)] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        }
        if (line.length > 16384) continue;
        NSURL* candidate = [NSURL URLWithString:line relativeToURL:_resolvedURL].absoluteURL;
        if (isHTTPURL(candidate) && choices.count < 16 && ![choices containsObject:candidate])
            [choices addObject:candidate];
    }
    NSURL* selected = choices.firstObject;
    if (!selected) { [self fail:@"playlist_no_http_stream" retry:NO]; return; }
    if ([_visited containsObject:selected.absoluteString])
    {
        [self fail:@"playlist_cycle" retry:NO];
        return;
    }
    [_visited addObject:selected.absoluteString];
    // PLS/M3U commonly list redundant servers. Retain bounded alternatives so
    // an unavailable first entry does not strand an otherwise valid station.
    for (NSUInteger index = 1; index < choices.count && _alternates.count < 16; ++index)
    {
        if (![_alternates containsObject:choices[index]]) [_alternates addObject:choices[index]];
    }
    [self disposePlayback];
    [self requestURL:selected];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
    if (task != _task || _closed || !_requested || _paused) return;
    _task = nil;
    _taskSuspended = false;
    if (error)
    {
        if (!_queueStarted && !_preferPlayer &&
            (error.code == NSURLErrorCannotParseResponse || error.code == NSURLErrorBadServerResponse))
        {
            [self useNativePlayer];
            return;
        }
        [self fail:[NSString stringWithFormat:@"network:%ld", static_cast<long>(error.code)] retry:YES];
        return;
    }
    if (!_bodyStarted && _prefix.length)
    {
        _bodyStarted = true;
        [self consumeBody:_prefix];
        _prefix = [NSMutableData data];
    }
    if (_isPlaylist) { [self resolvePlaylist]; return; }
    _endOfInput = true;
    [self drainPackets:YES];
    if (!_queue && !_player) [self fail:@"no_audio_packets" retry:NO];
}

- (void)streamProperty:(AudioFileStreamPropertyID)property
{
    if (property == kAudioFileStreamProperty_InfoDictionary)
    {
        CFDictionaryRef info = nullptr;
        UInt32 size = sizeof(info);
        if (AudioFileStreamGetProperty(_parser, property, &size, &info) == noErr && info)
        {
            NSDictionary* values = CFBridgingRelease(info);
            NSString* title = values[@(kAFInfoDictionary_Title)];
            NSString* artist = values[@(kAFInfoDictionary_Artist)];
            if ([title isKindOfClass:NSString.class]) _metadata[@"TITLE"] = title;
            if ([artist isKindOfClass:NSString.class]) _metadata[@"ARTIST"] = artist;
        }
        return;
    }
    if (property != kAudioFileStreamProperty_ReadyToProducePackets || _queue) return;
    UInt32 size = sizeof(_format);
    _parserFailure = AudioFileStreamGetProperty(_parser, kAudioFileStreamProperty_DataFormat, &size, &_format);
    if (_parserFailure) return;
    UInt32 listSize = 0;
    Boolean writable = false;
    if (AudioFileStreamGetPropertyInfo(_parser, kAudioFileStreamProperty_FormatList, &listSize, &writable) == noErr &&
        listSize && listSize <= 65536 && listSize % sizeof(AudioFormatListItem) == 0)
    {
        std::vector<AudioFormatListItem> list(listSize / sizeof(AudioFormatListItem));
        if (AudioFileStreamGetProperty(_parser, kAudioFileStreamProperty_FormatList, &listSize, list.data()) == noErr)
        {
            UInt32 selected = 0;
            UInt32 selectedSize = sizeof(selected);
            if (AudioFormatGetProperty(kAudioFormatProperty_FirstPlayableFormatFromList, listSize,
                                      list.data(), &selectedSize, &selected) == noErr && selected < list.size())
                _format = list[selected].mASBD;
        }
    }
    if (!std::isfinite(_format.mSampleRate) || _format.mSampleRate < 8000 || _format.mSampleRate > 384000 ||
        !_format.mChannelsPerFrame || _format.mChannelsPerFrame > 8)
    {
        _parserFailure = kAudioFileStreamError_UnsupportedDataFormat;
        return;
    }
    _output = std::make_unique<OutputState>();
    _parserFailure = AudioQueueNewOutput(&_format, outputBufferFinished, _output.get(), nullptr, nullptr, 0, &_queue);
    if (_parserFailure) return;
    _parserFailure = AudioQueueAddPropertyListener(_queue, kAudioQueueProperty_IsRunning,
        outputRunningChanged, _output.get());
    if (_parserFailure) return;
    AudioQueueCreateTimeline(_queue, &_timeline);
    AudioQueueSetParameter(_queue, kAudioQueueParam_Volume, _gain);
    [self applyOutputUID];
    UInt32 cookieSize = 0;
    if (AudioFileStreamGetPropertyInfo(_parser, kAudioFileStreamProperty_MagicCookieData, &cookieSize, &writable) == noErr &&
        cookieSize && cookieSize <= 65536)
    {
        std::vector<std::uint8_t> cookie(cookieSize);
        if (AudioFileStreamGetProperty(_parser, kAudioFileStreamProperty_MagicCookieData, &cookieSize, cookie.data()) == noErr)
            _parserFailure = AudioQueueSetProperty(_queue, kAudioQueueProperty_MagicCookie, cookie.data(), cookieSize);
        if (_parserFailure) return;
    }
    for (auto& slot : _output->slots)
    {
        _parserFailure = AudioQueueAllocateBuffer(_queue, BUFFER_BYTES, &slot.buffer);
        if (_parserFailure) return;
        slot.buffer->mUserData = &slot;
    }
}

- (UInt32)packetFrames:(const AudioStreamPacketDescription*)description bytes:(UInt32)bytes
{
    if (description && description->mVariableFramesInPacket) return description->mVariableFramesInPacket;
    if (_format.mFramesPerPacket) return _format.mFramesPerPacket;
    UInt32 bitRate = 0;
    UInt32 size = sizeof(bitRate);
    if (AudioFileStreamGetProperty(_parser, kAudioFileStreamProperty_BitRate, &size, &bitRate) == noErr && bitRate)
        _nominalBitRate = bitRate;
    // Some variable-frame formats expose packet duration only to AudioQueue.
    // This estimate controls prebuffering, never decoded timing or sample rate.
    const double estimate = _nominalBitRate > 0 ? bytes * 8.0 * _format.mSampleRate / _nominalBitRate :
        _format.mSampleRate * 0.02;
    return static_cast<UInt32>(std::clamp(estimate, 1.0, _format.mSampleRate));
}

- (void)streamPackets:(UInt32)bytes count:(UInt32)count data:(const void*)data
        descriptions:(AudioStreamPacketDescription*)descriptions
{
    if (!_queue || _parserFailure || !bytes || !count) return;
    if (count > MAX_PENDING_PACKETS || _packetBytes + bytes > MAX_PENDING_BYTES ||
        _packets.size() + count > MAX_PENDING_PACKETS || (!descriptions && bytes % count != 0))
    {
        _parserFailure = kAudioFileStreamError_UnspecifiedError;
        return;
    }
    const auto* raw = static_cast<const std::uint8_t*>(data);
    for (UInt32 index = 0; index < count; ++index)
    {
        const std::int64_t offset = descriptions ? descriptions[index].mStartOffset :
            static_cast<std::int64_t>(index) * (bytes / count);
        const UInt32 length = descriptions ? descriptions[index].mDataByteSize : bytes / count;
        if (offset < 0 || length > BUFFER_BYTES || static_cast<std::uint64_t>(offset) + length > bytes || !length)
        {
            _parserFailure = kAudioFileStreamError_InvalidPacketOffset;
            return;
        }
        CompressedPacket packet;
        packet.bytes.assign(raw + offset, raw + offset + length);
        packet.frames = [self packetFrames:descriptions ? &descriptions[index] : nullptr bytes:length];
        _pendingFrames += packet.frames;
        _packetBytes += length;
        _packets.push_back(std::move(packet));
    }
}

- (double)queuedSeconds
{
    if (!_output || !_format.mSampleRate) return 0;
    // A buffer-return callback releases compressed storage; a decoder can
    // return it before the corresponding device samples finish. Use the
    // output sample clock for the remaining playback duration, rather than
    // mistaking reusable input storage for an empty audible pipeline.
    const double enqueued = std::max(0.0, _totalFrames - _renderedSeconds * _format.mSampleRate);
    return (_pendingFrames + enqueued) / _format.mSampleRate;
}

- (void)drainPackets:(BOOL)force
{
    if (!_queue || !_output || _paused || _closed || _queueDraining) return;
    for (auto& slot : _output->slots)
    {
        if (_packets.empty()) break;
        if (!force && _pendingFrames < _format.mSampleRate * OUTPUT_BUFFER_SECONDS) break;
        if (!slot.available.load(std::memory_order_acquire)) continue;
        UInt32 bytes = 0;
        UInt32 count = 0;
        std::uint64_t frames = 0;
        std::array<AudioStreamPacketDescription, BUFFER_PACKETS> descriptions{};
        while (!_packets.empty() && count < BUFFER_PACKETS)
        {
            auto& packet = _packets.front();
            if (bytes + packet.bytes.size() > BUFFER_BYTES) break;
            std::memcpy(static_cast<std::uint8_t*>(slot.buffer->mAudioData) + bytes,
                        packet.bytes.data(), packet.bytes.size());
            descriptions[count] = {static_cast<SInt64>(bytes), packet.frames,
                                    static_cast<UInt32>(packet.bytes.size())};
            bytes += static_cast<UInt32>(packet.bytes.size());
            frames += packet.frames;
            _pendingFrames -= packet.frames;
            _packetBytes -= packet.bytes.size();
            _packets.pop_front();
            ++count;
            if (frames >= _format.mSampleRate * OUTPUT_BUFFER_SECONDS) break;
        }
        if (!count) continue;
        slot.frames = frames;
        slot.buffer->mAudioDataByteSize = bytes;
        slot.available.store(false, std::memory_order_release);
        _output->queuedFrames.fetch_add(frames, std::memory_order_relaxed);
        OSStatus status = AudioQueueEnqueueBuffer(_queue, slot.buffer, count, descriptions.data());
        if (status != noErr)
        {
            _output->queuedFrames.fetch_sub(frames, std::memory_order_relaxed);
            slot.available.store(true, std::memory_order_release);
            [self fail:[NSString stringWithFormat:@"queue_enqueue:%d", static_cast<int>(status)] retry:YES];
            return;
        }
        _totalFrames += frames;
    }
    const double queued = [self queuedSeconds];
    if (!_queueStarted && (queued >= START_BUFFER_SECONDS || (force && queued > 0)))
    {
        if (_queueEverStarted)
        {
            AudioTimeStamp stamp{};
            Boolean discontinuity = false;
            if (AudioQueueGetCurrentTime(_queue, _timeline, &stamp, _timeline ? &discontinuity : nullptr) == noErr &&
                (stamp.mFlags & kAudioTimeStampSampleTimeValid))
            {
                // The device clock may include silence during an underrun.
                // Preserve the media-frame origin when resuming the queue.
                _clockOffsetFrames = stamp.mSampleTime - _renderedSeconds * _format.mSampleRate;
                _lastClockSample = stamp.mSampleTime;
            }
        }
        OSStatus status = AudioQueueStart(_queue, nullptr);
        if (status != noErr)
        {
            [self fail:[NSString stringWithFormat:@"queue_start:%d", static_cast<int>(status)] retry:YES];
            return;
        }
        _queueStarted = true;
        _queueEverStarted = true;
    }
    if (_endOfInput && _packets.empty() && _queueStarted)
    {
        // Compressed packet frame counts can include encoder priming/padding
        // that the decoder trims. Native asynchronous stop drains the actual
        // decoder tail and is the completion receipt, rather than an estimated
        // remaining-frame count or an input-buffer return callback.
        _queueDraining = true;
        const OSStatus status = AudioQueueStop(_queue, false);
        if (status != noErr)
        {
            [self fail:[NSString stringWithFormat:@"queue_drain:%d", static_cast<int>(status)] retry:YES];
            return;
        }
    }
    if (_task && !_taskSuspended && (queued > HIGH_WATER_SECONDS || _packetBytes > MAX_PENDING_BYTES / 2))
    {
        [_task suspend];
        _taskSuspended = true;
    }
    else if (_task && _taskSuspended && queued < LOW_WATER_SECONDS && _packetBytes < MAX_PENDING_BYTES / 4)
    {
        [_task resume];
        _taskSuspended = false;
        _lastDataAt = [self now];
    }
}

- (void)useNativePlayer
{
    NSURL* url = _resolvedURL;
    [self disposePlayback];
    if (!_requested || _paused || !url) return;
    _preferPlayer = true;
    _state = @"buffering";
    _requestAt = _lastDataAt = [self now];
    AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:@{AVURLAssetHTTPUserAgentKey: @"Alchemy Native Audio"}];
    AVPlayerItem* item = [AVPlayerItem playerItemWithAsset:asset];
    _playerMetadata = [[AVPlayerItemMetadataOutput alloc] initWithIdentifiers:nil];
    [_playerMetadata setDelegate:self queue:_worker];
    [item addOutput:_playerMetadata];
    _player = [AVPlayer playerWithPlayerItem:item];
    _player.volume = _gain;
    [self applyOutputUID];
    [_player play];
}

- (void)metadataOutput:(AVPlayerItemMetadataOutput*)output
    didOutputTimedMetadataGroups:(NSArray<AVTimedMetadataGroup*>*)groups
    fromPlayerItemTrack:(AVPlayerItemTrack*)track
{
    if (output != _playerMetadata || _closed || !_requested || _paused) return;
    for (AVTimedMetadataGroup* group in groups)
    {
        for (AVMetadataItem* item in group.items)
        {
            NSString* value = item.stringValue;
            if (!value) continue;
            if ([item.identifier isEqualToString:AVMetadataIdentifierIcyMetadataStreamTitle]) [self setSongTitle:value];
            else if ([item.identifier isEqualToString:AVMetadataIdentifierIcyMetadataStreamURL]) _metadata[@"URL"] = value;
            else if ([item.commonKey isEqualToString:AVMetadataCommonKeyArtist]) _metadata[@"ARTIST"] = value;
            else if ([item.commonKey isEqualToString:AVMetadataCommonKeyTitle]) _metadata[@"TITLE"] = value;
        }
    }
}

- (void)advance
{
    if (_closed || !_requested || _paused) return;
    const double now = [self now];
    if (_retryAt)
    {
        if (now >= _retryAt)
        {
            _retryAt = 0;
            _visited = [NSMutableSet set];
            _playlistDepth = 0;
            [self requestURL:_resolvedURL];
        }
        return;
    }
    if (_player)
    {
        if (_player.currentItem.status == AVPlayerItemStatusFailed || _player.status == AVPlayerStatusFailed)
        {
            NSError* error = _player.currentItem.error ?: _player.error;
            [self fail:[NSString stringWithFormat:@"native_player:%ld", static_cast<long>(error.code)] retry:YES];
            return;
        }
        const double elapsed = CMTimeGetSeconds(_player.currentTime);
        if (std::isfinite(elapsed) && elapsed > _renderedSeconds)
        {
            _renderedSeconds = elapsed;
            _lastDataAt = now;
            if (!_firstOutputAt) _firstOutputAt = now - _requestAt;
        }
        _state = _player.timeControlStatus == AVPlayerTimeControlStatusPlaying && _renderedSeconds > 0 ?
            @"playing" : @"buffering";
        const double duration = CMTimeGetSeconds(_player.currentItem.duration);
        if (std::isfinite(duration) && duration > 0 && elapsed >= duration - 0.01)
        {
            _requested = false;
            _state = @"stopped";
            [self cancelTimer];
            return;
        }
    }
    else if (_queue)
    {
        [self drainPackets:_endOfInput];
        if (!_queue) return;
        AudioTimeStamp stamp{};
        Boolean discontinuity = false;
        if (AudioQueueGetCurrentTime(_queue, _timeline, &stamp, _timeline ? &discontinuity : nullptr) == noErr &&
            stamp.mFlags & kAudioTimeStampSampleTimeValid)
        {
            if (discontinuity || stamp.mSampleTime < _lastClockSample)
                _clockOffsetFrames = stamp.mSampleTime - _renderedSeconds * _format.mSampleRate;
            _lastClockSample = stamp.mSampleTime;
            const double frames = std::clamp(stamp.mSampleTime - _clockOffsetFrames, 0.0, _totalFrames);
            const double rendered = frames / _format.mSampleRate;
            if (rendered > _renderedSeconds)
            {
                _renderedSeconds = rendered;
                if (!_firstOutputAt) _firstOutputAt = now - _requestAt;
            }
        }
        const double queued = [self queuedSeconds];
        if (_queueDraining)
        {
            UInt32 running = 0;
            UInt32 size = sizeof(running);
            const OSStatus status = AudioQueueGetProperty(_queue, kAudioQueueProperty_IsRunning, &running, &size);
            if (status == noErr && running) _output->hasRun.store(true, std::memory_order_release);
            if (status == noErr && !running && _output->hasRun.load(std::memory_order_acquire))
            {
                _queueStarted = false;
                if (_finite)
                {
                    _totalFrames = _renderedSeconds * _format.mSampleRate;
                    _requested = false;
                    _state = @"stopped";
                    [self cancelTimer];
                }
                else [self fail:@"stream_ended" retry:YES];
                return;
            }
        }
        else if (_queueStarted && queued <= 0 && _packets.empty())
        {
            AudioQueuePause(_queue);
            _queueStarted = false;
        }
        _state = _queueStarted && _renderedSeconds > 0 ? @"playing" : @"buffering";
    }
    if (!_taskSuspended && now - _lastDataAt > 20)
    {
        [self fail:@"network_stall" retry:YES];
        return;
    }
    // A stable connection gets a fresh retry allowance, while the diagnostic
    // count remains the current connection's recovery history.
    if (_renderedSeconds >= 60)
    {
        _retries = 0;
        _attempts = 0;
    }
}

- (void)tick
{
    [self advance];
    [self publish];
}

- (void)publish
{
    // UI/audio polling is on the viewer thread. It must never synchronously
    // wait for network parsing, decoder setup or AudioQueue teardown.
    NSDictionary* metadata = [_metadata copy];
    NSDictionary* diagnostics = @{@"backend": _player ? @"AVFoundation" : @"CoreAudio",
        @"state": _state ?: @"stopped", @"rendered_seconds": @(_renderedSeconds),
        @"queued_seconds": @([self queuedSeconds]), @"sample_rate": @(_format.mSampleRate),
        @"retry_count": @(_retries), @"generation": @(_generation),
        @"error": _error ?: @"", @"output_device_uid": _outputUID ?: @"",
        @"first_output_seconds": @(_firstOutputAt), @"gain": @(_gain)};
    {
        std::lock_guard<std::mutex> lock(_snapshotMutex);
        _snapshotMetadata = metadata;
        _snapshotDiagnostics = diagnostics;
    }
    _snapshotPlaying.store(_paused ? 2 : _requested ? 1 : 0, std::memory_order_release);
}

- (int)playing
{
    return _snapshotPlaying.load(std::memory_order_acquire);
}

- (NSDictionary*)metadata
{
    NSDictionary* value;
    {
        std::lock_guard<std::mutex> lock(_snapshotMutex);
        value = _snapshotMetadata;
    }
    return value;
}

- (NSDictionary*)diagnostics
{
    NSDictionary* value;
    {
        std::lock_guard<std::mutex> lock(_snapshotMutex);
        value = _snapshotDiagnostics;
    }
    return value;
}
@end

struct LLStreamingAudio_CoreAudio::Impl
{
    __strong LLNativeMusic* native;
    std::string originalURL;
    F32 gain = 1.f;
    explicit Impl(const std::string& uid) : native([[LLNativeMusic alloc] initWithOutputUID:stringFromUTF8(uid)]) {}
    ~Impl() { [native close]; }
};

LLStreamingAudio_CoreAudio::LLStreamingAudio_CoreAudio(const std::string& output_device_uid) :
    mImpl(std::make_unique<Impl>(output_device_uid)), mMetadata(LLSD::emptyMap())
{
}

LLStreamingAudio_CoreAudio::~LLStreamingAudio_CoreAudio() = default;

void LLStreamingAudio_CoreAudio::start(const std::string& url)
{
    if (url.empty()) { stop(); return; }
    mImpl->originalURL = url;
    [mImpl->native startURL:cleanedURL(stringFromUTF8(url))];
}

void LLStreamingAudio_CoreAudio::stop()
{
    mImpl->originalURL.clear();
    [mImpl->native stop];
}

void LLStreamingAudio_CoreAudio::pause(int pause)
{
    const int state = isPlaying();
    if (state == 0 && mImpl->originalURL.empty()) return;
    const bool shouldPause = pause < 0 ? state == 1 : pause != 0;
    [mImpl->native setPaused:shouldPause];
}

void LLStreamingAudio_CoreAudio::update()
{
    LLSD metadata = getMetadata();
    const int state = isPlaying();
    if (state != mLastState || !llsd_equals(metadata, mMetadata))
    {
        mLastState = state;
        mMetadata = std::move(metadata);
        mMetadataUpdateSignal(mMetadata);
    }
}

int LLStreamingAudio_CoreAudio::isPlaying() { return [mImpl->native playing]; }

void LLStreamingAudio_CoreAudio::setGain(F32 gain)
{
    const F32 normalized = std::isfinite(gain) ? std::clamp(gain, 0.f, 1.f) : 0.f;
    if (normalized == mImpl->gain) return;
    mImpl->gain = normalized;
    [mImpl->native setGain:mImpl->gain];
}

F32 LLStreamingAudio_CoreAudio::getGain() { return mImpl->gain; }
std::string LLStreamingAudio_CoreAudio::getURL() { return mImpl->originalURL; }
LLSD LLStreamingAudio_CoreAudio::getMetadata() const { return dictionaryToLLSD([mImpl->native metadata]); }
void LLStreamingAudio_CoreAudio::setOutputDevice(const std::string& uid) { [mImpl->native setOutputUID:stringFromUTF8(uid)]; }
LLSD LLStreamingAudio_CoreAudio::getDiagnostics() const { return dictionaryToLLSD([mImpl->native diagnostics]); }
