/**
 * @file llworkerthread_test.cpp
 * @brief Bounded admission, initialization rollback and worker ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "llworkerthread.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
struct Counts
{
    int started = 0, processed = 0, finished = 0, ended = 0, rolled_back = 0, deleted = 0;
    bool initialized = false, ready_when_processed = true;
};

class Thread final : public LLWorkerThread
{
public:
    explicit Thread(size_t capacity) : LLWorkerThread("worker admission", true, false, capacity, 1) {}
    ~Thread() override { shutdown(); clearDeleteList(); }
    bool one() { return processOne(); }
};

class Worker final : public LLWorkerClass
{
public:
    Worker(Thread& thread, Counts& counts) : LLWorkerClass(&thread, "admission worker"), mCounts(counts) {}
    ~Worker() override { ++mCounts.deleted; }
    bool submit() { return addWork(17); }
    bool collect() { return checkWork(); }
    bool doWork(S32 param) override
    {
        ++mCounts.processed;
        mCounts.ready_when_processed &= mCounts.initialized && param == 17;
        return true;
    }
    void finishWork(S32 param, bool complete) override
    {
        ++mCounts.finished;
        mCounts.ready_when_processed &= mCounts.initialized && param == 17 && complete;
    }
private:
    void startWork(S32) override { ++mCounts.started; mCounts.initialized = true; }
    void endWork(S32, bool aborted) override
    {
        ++mCounts.ended;
        mCounts.rolled_back += aborted;
        mCounts.initialized = false;
    }
    Counts& mCounts;
};
}

TEST_CASE("worker admission: unreaped completion retains capacity and denial rolls initialization back", "[workerthread]")
{
    Counts first_counts, second_counts;
    Thread thread(1);
    auto* first = new Worker(thread, first_counts);
    auto* second = new Worker(thread, second_counts);
    REQUIRE(first->submit());
    REQUIRE(thread.one());
    CHECK(first_counts.ready_when_processed);
    CHECK(first_counts.finished == 1);
    CHECK_FALSE(second->submit());
    CHECK_FALSE(second->haveWork());
    CHECK_FALSE(second->wasAborted());
    CHECK(second->getFlags(LLWorkerClass::WCF_WORK_FINISHED));
    CHECK(second_counts.started == 1);
    CHECK(second_counts.rolled_back == 1);
    CHECK(second_counts.finished == 0);
    CHECK(second_counts.processed == 0);
    REQUIRE(first->collect());
    REQUIRE(second->submit());
    REQUIRE(thread.one());
    REQUIRE(second->collect());
    CHECK(second_counts.started == 2);
    CHECK(second_counts.ended == 2);
    CHECK(second_counts.finished == 1);
    CHECK(second_counts.ready_when_processed);
    first->scheduleDelete();
    second->scheduleDelete();
    thread.update(0);
    CHECK(first_counts.deleted == 1);
    CHECK(second_counts.deleted == 1);
    CHECK(thread.getNumDeletes() == 0);
}

TEST_CASE("worker admission: rejected and closed requests dispose without fabricated completion", "[workerthread]")
{
    Counts blocker_counts, rejected_counts, closed_counts;
    Thread thread(1);
    auto* blocker = new Worker(thread, blocker_counts);
    REQUIRE(blocker->submit());
    auto* rejected = new Worker(thread, rejected_counts);
    REQUIRE_FALSE(rejected->submit());
    rejected->discardUnaccepted();
    CHECK(rejected_counts.started == 1);
    CHECK(rejected_counts.rolled_back == 1);
    CHECK(rejected_counts.finished == 0);
    CHECK(rejected_counts.deleted == 1);
    CHECK(thread.getNumDeletes() == 0);
    REQUIRE(thread.one());
    REQUIRE(blocker->collect());
    blocker->scheduleDelete();
    thread.update(0);
    CHECK(blocker_counts.processed == 1);
    CHECK(blocker_counts.finished == 1);
    CHECK(blocker_counts.deleted == 1);
    CHECK(blocker_counts.ready_when_processed);
    CHECK(thread.getNumDeletes() == 0);
    thread.shutdown();
    auto* closed = new Worker(thread, closed_counts);
    REQUIRE_FALSE(closed->submit());
    closed->discardUnaccepted();
    CHECK(closed_counts.finished == 0);
    CHECK(closed_counts.deleted == 1);
}

TEST_CASE("worker admission: a never-submitted dormant owner can be deleted", "[workerthread]")
{
    Counts counts;
    Thread thread(1);
    auto* worker = new Worker(thread, counts);
    worker->scheduleDelete();
    thread.update(0);
    CHECK(counts.started == 0);
    CHECK(counts.finished == 0);
    CHECK(counts.deleted == 1);
    CHECK(thread.getNumDeletes() == 0);
}
