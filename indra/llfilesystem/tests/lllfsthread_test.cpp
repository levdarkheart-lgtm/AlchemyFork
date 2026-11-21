/**
 * @file lllfsthread_test.cpp
 * @brief Nonblocking file admission and exactly-once accepted completion.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "lllfsthread.h"
#include "lluuid.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace
{
struct Counts { int called = 0, deleted = 0; S32 bytes = -1; };
class Responder final : public LLLFSThread::Responder
{
public:
    explicit Responder(Counts& counts) : mCounts(counts) {}
    ~Responder() override { ++mCounts.deleted; }
    void completed(S32 bytes) override { ++mCounts.called; mCounts.bytes = bytes; }
private:
    Counts& mCounts;
};
class Thread final : public LLLFSThread
{
public:
    explicit Thread(size_t capacity) : LLLFSThread(true, capacity, 1) {}
    ~Thread() override { shutdown(); }
    bool one() { return processOne(); }
};
struct TempFile
{
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("alchemy-lfs-" + LLUUID::generateNewID().asString());
    ~TempFile() { std::error_code ec; std::filesystem::remove(path, ec); }
};
}

TEST_CASE("LFS admission: busy writes retain caller data and do not fabricate completion", "[lfsthread]")
{
    Counts accepted, rejected, retried;
    Thread thread(1);
    TempFile file;
    U8 data[] = {1, 2, 3, 4};
    LLLFSThread::Admission admission;
    REQUIRE(thread.write(file.path.string(), data, 0, sizeof(data), new Responder(accepted), &admission));
    CHECK(admission == LLLFSThread::Admission::ACCEPTED);
    CHECK(thread.write(file.path.string(), data, 0, sizeof(data), new Responder(rejected), &admission) == 0);
    CHECK(admission == LLLFSThread::Admission::BUSY);
    CHECK(rejected.called == 0);
    CHECK(rejected.deleted == 1);
    REQUIRE(thread.one());
    CHECK(accepted.called == 1);
    CHECK(accepted.bytes == sizeof(data));
    CHECK(accepted.deleted == 1);
    REQUIRE(thread.write(file.path.string(), data, 0, sizeof(data), new Responder(retried), &admission));
    REQUIRE(thread.one());
    CHECK(retried.called == 1);
    CHECK(retried.deleted == 1);
    std::ifstream input(file.path, std::ios::binary);
    const std::vector<U8> stored{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    CHECK(stored == std::vector<U8>{1, 2, 3, 4});
    CHECK(thread.getPending() == 0);
}

TEST_CASE("LFS admission: accepted abort completes once but closed rejection never calls back", "[lfsthread]")
{
    Counts aborted, closed;
    Thread thread(1);
    TempFile file;
    U8 data[4] = {};
    const auto handle = thread.read(file.path.string(), data, 0, sizeof(data), new Responder(aborted));
    REQUIRE(handle != 0);
    thread.abortRequest(handle, true);
    REQUIRE(thread.one());
    CHECK(aborted.called == 1);
    CHECK(aborted.bytes == 0);
    CHECK(aborted.deleted == 1);
    thread.shutdown();
    LLLFSThread::Admission admission;
    CHECK(thread.read(file.path.string(), data, 0, sizeof(data), new Responder(closed), &admission) == 0);
    CHECK(admission == LLLFSThread::Admission::CLOSED);
    CHECK(closed.called == 0);
    CHECK(closed.deleted == 1);
    CHECK_FALSE(std::filesystem::exists(file.path));
}
