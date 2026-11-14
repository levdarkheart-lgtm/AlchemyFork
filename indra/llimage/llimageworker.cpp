/**
 * @file llimageworker.cpp
 * @brief Base class for images.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llimageworker.h"
#include "alnativeframebudget.h"
#include "llimagedxt.h"
#include "threadpool.h"
#if LL_DARWIN
#include "llsys_objc.h"
#endif

#include <chrono>
#include <cxxabi.h>
#include <thread>

/*--------------------------------------------------------------------------*/
class ImageRequest
{
public:
    ImageRequest(const LLPointer<LLImageFormatted>& image,
                 S32 discard,
                 bool needs_aux,
                 const LLPointer<LLImageDecodeThread::Responder>& responder,
                 U32 request_id);
    virtual ~ImageRequest();

    /*virtual*/ bool processRequest();
    /*virtual*/ void finishRequest(bool completed);
    void setStage(const char* stage) noexcept { mStage = stage; }
    void logUnexpectedException(const char* type, const char* message) const noexcept;

private:
    // LLPointers stored in ImageRequest MUST be LLPointer instances rather
    // than references: we need to increment the refcount when storing these.
    // input
    LLPointer<LLImageFormatted> mFormattedImage;
    S32 mDiscardLevel;
    U32 mRequestId;
    bool mNeedsAux;
    // output
    LLPointer<LLImageRaw> mDecodedImageRaw;
    LLPointer<LLImageRaw> mDecodedImageAux;
    bool mDecodedRaw;
    bool mDecodedAux;
    LLPointer<LLImageDecodeThread::Responder> mResponder;
    std::string mErrorString;
    // Saved under the image lock, never rediscovered while handling a throw.
    const char* mStage = "queued";
    S32 mInputCodec = -1;
    S32 mInputBytes = 0;
    S32 mInputWidth = 0;
    S32 mInputHeight = 0;
    S32 mInputComponents = 0;
};


//----------------------------------------------------------------------------

// MAIN THREAD
LLImageDecodeThread::LLImageDecodeThread(bool /*threaded*/)
    : LLImageDecodeThread(true, ALImageDecodeScheduler::Limits::fromTopology(
          std::thread::hardware_concurrency(), {}))
{}

LLImageDecodeThread::LLImageDecodeThread(bool /*threaded*/,
                                       ALImageDecodeScheduler::Limits limits,
                                       bool adaptive,
                                       ALImageDecodeScheduler::Now now)
    : mNow(now ? std::move(now) : ALImageDecodeScheduler::Now(ALImageDecodeScheduler::Clock::now)),
      mScheduler(std::make_shared<ALImageDecodeScheduler>(limits, adaptive, mNow)),
      mDecodeCount(0)
{
    // The published pool width and admission ceiling must agree. The viewer
    // derives limits from live topology before constructing this pool.
    mThreadPool = std::make_unique<LL::ThreadPool>(
        "ImageDecode", mScheduler->snapshot().maximum, 1024 * 1024, true, true);
    mThreadPool->start();
}

//virtual
LLImageDecodeThread::~LLImageDecodeThread()
{
    shutdown();
}

// MAIN THREAD
// virtual
size_t LLImageDecodeThread::update(F32 max_time_ms)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    updateFrameAdmission(ALNativeFrameBudget::instance());
    const auto now = mNow();
    if (!mHaveControlUpdate || now - mLastControlUpdate >= std::chrono::milliseconds(500))
    {
        mHaveControlUpdate = true;
        mLastControlUpdate = now;
        ALImageDecodeScheduler::Pressure pressure;
#if LL_DARWIN
        const auto native = LLGetDarwinDecodePressure();
        pressure.system_busy = native.system_cpu_busy;
        pressure.main_thread_busy = native.main_thread_cpu_busy;
        pressure.cpu_valid = native.system_cpu_busy >= 0. && native.main_thread_cpu_busy >= 0.;
        pressure.thermal_level = native.thermal_state;
        pressure.low_power = native.low_power;
#endif
        updateConcurrency(pressure);
    }
    return getPending();
}

size_t LLImageDecodeThread::getPending()
{
    const auto stats = mScheduler->snapshot();
    return stats.waiting + stats.active;
}

ALImageDecodeScheduler::Snapshot LLImageDecodeThread::getDecodeStats() const
{
    return mScheduler->snapshot();
}

void LLImageDecodeThread::updateFrameAdmission(const ALNativeFrameBudget& budget)
{
    const auto admission = mScheduler->snapshot();
    mScheduler->setDownstreamCeiling(budget.decodeCeiling(
        admission.minimum, admission.current, admission.maximum));
}

void LLImageDecodeThread::updateConcurrency(const ALImageDecodeScheduler::Pressure& pressure)
{
    const auto now = mNow();
    mScheduler->tick(now, pressure);
    const auto stats = mScheduler->snapshot(now);
    if (!mHaveStatsLog || stats.current != mLastLoggedLimit ||
        now - mLastStatsLog >= std::chrono::seconds(5))
    {
        mHaveStatsLog = true;
        mLastStatsLog = now;
        mLastLoggedLimit = stats.current;
        LL_INFOS("ImageDecode") << "Decode admission: adaptive=" << stats.adaptive
            << " limit=" << stats.current << " minimum=" << stats.minimum
            << " normal=" << stats.baseline << " maximum=" << stats.maximum
            << " downstream_ceiling=" << stats.downstream_ceiling
            << " waiting=" << stats.waiting << " active=" << stats.active
            << " oldest_wait_ms=" << stats.oldest_wait_s * 1000.
            << " submitted=" << stats.submitted << " started=" << stats.started
            << " completed=" << stats.completed << " cancelled=" << stats.cancelled
            << " queue_wait_count=" << stats.queue_wait.count
            << " queue_wait_ms_sum=" << stats.queue_wait.total_s * 1000.
            << " queue_wait_ms_max=" << stats.queue_wait.max_s * 1000.
            << " decode_wall_count=" << stats.decode_wall.count
            << " decode_wall_ms_sum=" << stats.decode_wall.total_s * 1000.
            << " decode_wall_ms_max=" << stats.decode_wall.max_s * 1000.
            << " service_wall_ms_sum=" << stats.service_wall.total_s * 1000.
            << " occupancy=" << stats.last_interval_occupancy
            << " saturated_backlog_ms=" << stats.last_interval_saturated_backlog_s * 1000.
            << " window_s=" << stats.window.duration_s
            << " window_started=" << stats.window.started
            << " window_completed=" << stats.window.completed
            << " window_queue_wait_ms_sum=" << stats.window.queue_wait.total_s * 1000.
            << " window_queue_wait_ms_max=" << stats.window.queue_wait.max_s * 1000.
            << " window_decode_wall_ms_sum=" << stats.window.decode_wall.total_s * 1000.
            << " system_cpu_busy=" << pressure.system_busy
            << " main_thread_cpu_busy=" << pressure.main_thread_busy
            << " thermal=" << pressure.thermal_level << " low_power=" << pressure.low_power
            << " reason=" << stats.transition_reason << LL_ENDL;
    }
}

LLImageDecodeThread::handle_t LLImageDecodeThread::decodeImage(
    const LLPointer<LLImageFormatted>& image,
    S32 discard,
    bool needs_aux,
    const LLPointer<LLImageDecodeThread::Responder>& responder)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    U32 decode_id = ++mDecodeCount;
    if (decode_id == 0)
        decode_id = ++mDecodeCount;

    const auto scheduler = mScheduler;
    const auto ticket = scheduler->submit();
    if (!ticket)
    {
        return 0;
    }
    bool posted = false;
    try
    {
        posted = mThreadPool->getQueue().post(
            [req = ImageRequest(image, discard, needs_aux, responder, decode_id), scheduler, ticket]
            () mutable
            {
                try
                {
                    req.setStage("admission");
                    auto permit = scheduler->acquire(ticket);
                    if (!permit)
                    {
                        req.finishRequest(false);
                        return;
                    }
                    // Keep unexpected failures on the existing WorkQueue
                    // propagation path. Record the phase before unwinding
                    // erases the worker's useful context (PPD-090).
                    const auto done = req.processRequest();
                    req.setStage("completion-accounting");
                    permit.decoded();
                    req.finishRequest(done);
                }
                catch (const std::exception& error)
                {
                    req.logUnexpectedException(typeid(error).name(), error.what());
                    throw;
                }
                catch (...)
                {
                    const auto* type = abi::__cxa_current_exception_type();
                    req.logUnexpectedException(type ? type->name() : "unknown",
                                               "non-standard exception");
                    throw;
                }
            });
    }
    catch (...)
    {
        scheduler->cancel(ticket);
        throw;
    }
    if (! posted)
    {
        scheduler->cancel(ticket);
        LL_DEBUGS() << "Tried to start decoding on shutdown" << LL_ENDL;
        return 0;
    }

    return decode_id;
}

void LLImageDecodeThread::shutdown()
{
    // Admission waiters already own accepted requests. Release their gate and
    // drain them before joining the pool, including when no more frame ticks run.
    mScheduler->beginDrain();
    mThreadPool->close();
}

LLImageDecodeThread::Responder::~Responder()
{
}

//----------------------------------------------------------------------------

ImageRequest::ImageRequest(const LLPointer<LLImageFormatted>& image,
                           S32 discard,
                           bool needs_aux,
                           const LLPointer<LLImageDecodeThread::Responder>& responder,
                           U32 request_id)
    : mFormattedImage(image),
      mDiscardLevel(discard),
      mNeedsAux(needs_aux),
      mDecodedRaw(false),
      mDecodedAux(false),
      mResponder(responder),
      mRequestId(request_id)
{
}

ImageRequest::~ImageRequest()
{
    mDecodedImageRaw = NULL;
    mDecodedImageAux = NULL;
    mFormattedImage = NULL;
}

void ImageRequest::logUnexpectedException(
    const char* type, const char* message) const noexcept
{
    // Diagnostics must neither replace the original exception nor call back
    // into image/codec code after a failed lock, allocation or responder.
    try
    {
        LL_WARNS("ImageDecode") << "Unexpected image request exception: request="
            << mRequestId << ", stage=" << mStage << ", codec=" << mInputCodec
            << ", bytes=" << mInputBytes << ", width=" << mInputWidth
            << ", height=" << mInputHeight << ", components=" << mInputComponents
            << ", discard=" << mDiscardLevel << ", aux=" << mNeedsAux
            << ", type=" << LLError::Log::demangle(type)
            << ", what=" << (message ? message : "(null)") << LL_ENDL;
    }
    catch (...)
    {
        // In particular, an allocation failure while logging must leave the
        // pending exception and WorkQueue's fatal/error policy untouched.
    }
}

//----------------------------------------------------------------------------


// Returns true when done, whether or not decode was successful.
bool ImageRequest::processRequest()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    if (mFormattedImage.isNull())
        return true;

    const F32 decode_time_slice = 0.f; //disable time slicing
    bool done = true;

    mStage = "formatted-lock";
    LLImageDataLock lockFormatted(mFormattedImage);
    mStage = "raw-lock";
    LLImageDataLock lockDecodedRaw(mDecodedImageRaw);
    mStage = "aux-lock";
    LLImageDataLock lockDecodedAux(mDecodedImageAux);
    mInputCodec = mFormattedImage->getCodec();
    mInputBytes = mFormattedImage->getDataSize();
    mInputWidth = mFormattedImage->getWidth();
    mInputHeight = mFormattedImage->getHeight();
    mInputComponents = mFormattedImage->getComponents();

    if (!mDecodedRaw)
    {
        // Decode primary channels
        if (mDecodedImageRaw.isNull())
        {
            // parse formatted header
            mStage = "metadata";
            if (!mFormattedImage->updateData())
            {
                return true; // done (failed)
            }
            if ((mFormattedImage->getWidth() * mFormattedImage->getHeight() * mFormattedImage->getComponents()) == 0)
            {
                return true; // done (failed)
            }
            if (mDiscardLevel >= 0)
            {
                mStage = "discard-selection";
                mFormattedImage->setDiscardLevel(mDiscardLevel);
            }
            mInputWidth = mFormattedImage->getWidth();
            mInputHeight = mFormattedImage->getHeight();
            mInputComponents = mFormattedImage->getComponents();
            mStage = "primary-allocation";
            mDecodedImageRaw = new LLImageRaw(mFormattedImage->getWidth(),
                                              mFormattedImage->getHeight(),
                                              mFormattedImage->getComponents());
        }
        mStage = "primary-decode";
        done = mFormattedImage->decode(mDecodedImageRaw, decode_time_slice);
        // some decoders are removing data when task is complete and there were errors
        mDecodedRaw = done && mDecodedImageRaw->getData();

        // Pick up errors from decoding
        mStage = "primary-error-copy";
        mErrorString = LLImage::getLastThreadError();
    }
    if (done && mNeedsAux && !mDecodedAux && mFormattedImage.notNull())
    {
        // Decode aux channel
        if (!mDecodedImageAux)
        {
            mStage = "aux-allocation";
            mDecodedImageAux = new LLImageRaw(mFormattedImage->getWidth(),
                                              mFormattedImage->getHeight(),
                                              1);
        }
        mStage = "aux-decode";
        done = mFormattedImage->decodeChannels(mDecodedImageAux, decode_time_slice, 4, 4);
        mDecodedAux = done && mDecodedImageAux->getData();

        // Pick up errors from decoding
        mStage = "aux-error-copy";
        mErrorString = LLImage::getLastThreadError();
    }

    return done;
}

void ImageRequest::finishRequest(bool completed)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    if (mResponder.notNull())
    {
        mStage = "responder";
        bool success = completed && mDecodedRaw && (!mNeedsAux || mDecodedAux);
        mResponder->completed(success, mErrorString, mDecodedImageRaw, mDecodedImageAux, mRequestId);
    }
    // Will automatically be deleted
}
