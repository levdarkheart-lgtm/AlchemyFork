/**
 * @file llstreamingaudio_coreaudio.h
 * @brief Native macOS parcel-radio playback. Voice and viewer sounds have separate owners.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License, version 2.1 only.
 */

#ifndef LL_STREAMINGAUDIO_COREAUDIO_H
#define LL_STREAMINGAUDIO_COREAUDIO_H

#include "llstreamingaudio.h"

#include <memory>
#include <string>

class LLStreamingAudio_CoreAudio final : public LLStreamingAudioInterface
{
public:
    explicit LLStreamingAudio_CoreAudio(const std::string& output_device_uid = {});
    ~LLStreamingAudio_CoreAudio() override;

    void start(const std::string& url) override;
    void stop() override;
    void pause(int pause) override;
    void update() override;
    int isPlaying() override;
    void setGain(F32 gain) override;
    F32 getGain() override;
    std::string getURL() override;

    bool supportsMetaData() override { return true; }
    LLSD getMetadata() const override;
    bool supportsWaveData() override { return false; }
    bool getWaveData(float*, S32, S32 = 1) override { return false; }

    // Core Audio UID, not the FAudio device-list identifier. Empty follows the
    // system output. This never changes the system default or any voice device.
    void setOutputDevice(const std::string& output_device_uid);

    // Bounded local playback diagnostics, without network URLs or credentials.
    // state: stopped/connecting/buffering/playing/paused/reconnecting/failed.
    // rendered_seconds, queued_seconds, sample_rate, retry_count, generation,
    // error (stable stage/code text), output_device_uid, backend.
    LLSD getDiagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
    LLSD mMetadata;
    S32 mLastState = 0;
};

#endif
