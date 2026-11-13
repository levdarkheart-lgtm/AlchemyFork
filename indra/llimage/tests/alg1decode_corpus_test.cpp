/**
 * @file alg1decode_corpus_test.cpp
 * @brief Opt-in PPD-090 reproduction through the real image decode workers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"

#include "llimagej2c.h"
#include "llimageworker.h"
#include "workqueue.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

struct CorpusCase
{
    std::string id;
    std::filesystem::path path;
    std::vector<U8> bytes;
    S32 discard = 0;
};

// A stable local correlation key, not a cryptographic anonymity guarantee.
std::string pathFingerprint(const std::string& path)
{
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : path)
    {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::vector<CorpusCase> readCorpus(const std::filesystem::path& manifest)
{
    std::ifstream input(manifest);
    if (!input) throw std::runtime_error("Cannot open the decode corpus manifest");
    std::vector<CorpusCase> cases;
    std::set<std::string> ids, paths;
    std::size_t total_bytes = 0;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        const auto first = line.find('\t'), last = line.rfind('\t');
        if (first == std::string::npos || first == last ||
            line.find('\t', first + 1) != last)
            throw std::runtime_error("Expected case_id, path and discard TSV columns");
        CorpusCase item;
        item.id = line.substr(0, first);
        if (item.id.empty() || item.id.size() > 64 ||
            item.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos ||
            !ids.insert(item.id).second)
            throw std::runtime_error("Case IDs must be unique anonymized identifiers");
        const auto reduction = line.substr(last + 1);
        if (reduction != "0" && reduction != "1" && reduction != "3")
            throw std::runtime_error("The bounded decode corpus accepts discard 0, 1 or 3");
        item.discard = reduction.front() - '0';
        item.path = line.substr(first + 1, last - first - 1);
        if (item.path.empty()) throw std::runtime_error("Empty corpus input path");
        if (item.path.is_relative()) item.path = manifest.parent_path() / item.path;
        item.path = item.path.lexically_normal();
        paths.insert(item.path.string());
        if (cases.size() == 240 || paths.size() > 80)
            throw std::runtime_error("Corpus exceeds 80 input files or 240 decode requests");
        std::ifstream file(item.path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot read corpus case " + item.id);
        const auto size = file.tellg();
        if (size <= 0 || size > 16 * 1024 * 1024)
            throw std::runtime_error("Corpus input must contain 1 byte to 16 MiB: " + item.id);
        total_bytes += static_cast<std::size_t>(size);
        if (total_bytes > 256 * 1024 * 1024)
            throw std::runtime_error("Corpus exceeds 256 MiB of queued encoded inputs");
        item.bytes.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(item.bytes.data()), size))
            throw std::runtime_error("Cannot finish reading corpus case " + item.id);
        cases.push_back(std::move(item));
    }
    if (cases.empty()) throw std::runtime_error("The decode corpus is empty");
    return cases;
}

std::string safeDetail(std::string message, const std::vector<CorpusCase>& cases,
                       const std::filesystem::path& manifest)
{
    // Codec inputs are supplied as bytes, never filenames. Also redact known
    // paths if an unexpected exception includes one, and keep records one-line.
    auto redact = [&](const std::string& path)
    {
        for (std::size_t at = 0; !path.empty() && (at = message.find(path, at)) != std::string::npos;)
        {
            message.replace(at, path.size(), "<input-path>");
            at += 12;
        }
    };
    redact(manifest.string());
    for (const auto& item : cases) redact(item.path.string());
    if (const char* home = std::getenv("HOME")) redact(home);
    std::ostringstream escaped;
    for (unsigned char c : message.substr(0, 1024))
    {
        if (c == '\\' || c == '"') escaped << '\\' << char(c);
        else if (c < 32 || c == 127) escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
        else escaped << char(c);
    }
    return escaped.str();
}

struct Completion
{
    std::size_t row = 0;
    U32 request = 0;
    bool success = false;
    U16 width = 0, height = 0;
    S8 components = 0;
    std::string error;
};

struct CompletionState
{
    std::mutex mutex;
    std::vector<Completion> values;
};

class CorpusResponder final : public LLImageDecodeThread::Responder
{
public:
    CorpusResponder(std::shared_ptr<CompletionState> state, std::size_t row)
        : mState(std::move(state)), mRow(row) {}

    void completed(bool success, const std::string& error, LLImageRaw* raw,
                   LLImageRaw*, U32 request) override
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        mState->values.push_back({mRow, request, success,
            raw ? raw->getWidth() : U16(0), raw ? raw->getHeight() : U16(0),
            raw ? raw->getComponents() : S8(0), error});
    }
private:
    std::shared_ptr<CompletionState> mState;
    std::size_t mRow;
};

struct EscapedException
{
    std::string stage, type, detail, id;
};

struct RunResult
{
    std::vector<Completion> completions;
    std::vector<EscapedException> exceptions;
    std::set<std::size_t> submitted;
    bool timed_out = false;
};

RunResult runWorkers(const std::vector<CorpusCase>& cases,
                     const std::vector<std::size_t>& rows,
                     LL::WorkQueue& mainloop, Clock::time_point deadline)
{
    RunResult result;
    const auto state = std::make_shared<CompletionState>();
    // Fixed six-way admission exercises the owner's widened production path.
    // The object drains and joins before the caller can destroy mainloop or
    // responder state, including when a fixture allocation throws.
    LLImageDecodeThread workers(true, {6, 6, 6, 6, 6, true}, false);
    auto capture = [&](const char* stage, const auto& action,
                       const std::string& id = "unassociated")
    {
        try { action(); }
        catch (const std::exception& error)
        {
            result.exceptions.push_back({stage, typeid(error).name(), error.what(), id});
        }
        catch (...)
        {
            result.exceptions.push_back({stage, "non_std", "Non-std exception", id});
        }
    };
    auto pump = [&]
    {
        while (mainloop.size()) capture("mainloop", [&] { mainloop.runOne(); });
    };
    for (const auto row : rows)
    {
        capture("submit", [&]
        {
            const auto& item = cases[row];
            LLPointer<LLImageFormatted> image = new LLImageJ2C;
            auto* data = image->allocateData(static_cast<S32>(item.bytes.size()));
            if (!data) throw std::runtime_error("Encoded input allocation failed");
            std::memcpy(data, item.bytes.data(), item.bytes.size());
            // Do not call load()/updateData() here: production ImageRequest
            // must parse the real OpenJPEG metadata on its decode worker.
            if (!workers.decodeImage(image, item.discard, false,
                                     new CorpusResponder(state, row)))
                throw std::runtime_error("Production decode queue rejected a request");
            result.submitted.insert(row);
        }, cases[row].id);
    }
    while (workers.getPending() && Clock::now() < deadline)
    {
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    result.timed_out = workers.getPending() != 0;
    // Production close cannot interrupt an in-flight codec. The external test
    // runner must retain its hard process timeout if a codec actually hangs.
    workers.shutdown();
    // Join closes the small race between the last admission release and its
    // WorkQueue exception post; every escaped worker exception is now queued.
    pump();
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        result.completions = state->values;
    }
    return result;
}

void printExceptions(const RunResult& result, const char* phase,
                     const std::vector<CorpusCase>& cases,
                     const std::filesystem::path& manifest,
                     const std::string& id = "unassociated")
{
    for (const auto& error : result.exceptions)
        std::cout << "G1_DECODE_EXCEPTION phase=" << phase << " case="
            << (error.id == "unassociated" ? id : error.id)
            << " stage=" << error.stage << " type=" << error.type
            << " what=\"" << safeDetail(error.detail, cases, manifest) << "\"\n";
}
}

TEST_CASE("G1 private corpus exercises real JPEG2000 workers and main-loop exception propagation",
          "[.][g1-decode-corpus]")
{
    const char* path = std::getenv("ALCHEMY_DECODE_CORPUS_TSV");
    if (!path || !*path) SKIP("Set ALCHEMY_DECODE_CORPUS_TSV to an anonymized private TSV");
    REQUIRE(LLImageJ2C::getEngineInfo().find("OpenHTJ2K 0.27.0 decode; ") == 0);
    REQUIRE_FALSE(LL::WorkQueue::getInstance("mainloop"));
    const std::filesystem::path manifest(path);
    const auto cases = readCorpus(manifest);
    LL::WorkQueue mainloop("mainloop", 1024, false);
    std::vector<std::size_t> rows;
    for (std::size_t row = 0; row < cases.size(); ++row) rows.push_back(row);
    std::cout << "G1_DECODE_BEGIN requests=" << cases.size()
        << " workers=6 auxiliary=0 manifest_path_fnv1a64=" << pathFingerprint(manifest.string())
        << " cooperative_batch_timeout_s=60 outer_process_timeout_required=1\n";
    const auto run = runWorkers(cases, rows, mainloop, Clock::now() + std::chrono::seconds(60));
    printExceptions(run, "parallel", cases, manifest);
    std::set<std::size_t> completed;
    std::size_t successes = 0, failures = 0, duplicates = 0;
    for (const auto& value : run.completions)
    {
        if (!completed.insert(value.row).second) ++duplicates;
        successes += value.success;
        failures += !value.success;
        const auto& item = cases[value.row];
        std::cout << "G1_DECODE_RESULT phase=parallel case=" << item.id
            << " path_fnv1a64=" << pathFingerprint(item.path.string())
            << " bytes=" << item.bytes.size() << " discard=" << item.discard
            << " request=" << value.request << " success=" << value.success
            << " width=" << value.width << " height=" << value.height
            << " components=" << int(value.components)
            << " error=\"" << safeDetail(value.error, cases, manifest) << "\"\n";
    }
    std::vector<std::size_t> missing;
    for (const auto row : rows)
        if (!completed.count(row))
        {
            missing.push_back(row);
            const auto& item = cases[row];
            std::cout << "G1_DECODE_NO_RESPONDER case=" << item.id
                << " path_fnv1a64=" << pathFingerprint(item.path.string())
                << " bytes=" << item.bytes.size() << " discard=" << item.discard << '\n';
        }
    // A throwing ImageRequest never calls its responder. Rerun only those
    // candidates, one at a time, without intercepting or altering codec calls.
    const auto narrow_deadline = Clock::now() + std::chrono::seconds(30);
    if (!run.exceptions.empty() && !run.timed_out && missing.size() <= 16)
        for (const auto row : missing)
        {
            if (Clock::now() >= narrow_deadline) break;
            const auto serial = runWorkers(cases, {row}, mainloop,
                std::min(narrow_deadline, Clock::now() + std::chrono::seconds(5)));
            printExceptions(serial, "serial", cases, manifest, cases[row].id);
            std::cout << "G1_DECODE_NARROW case=" << cases[row].id
                << " callbacks=" << serial.completions.size()
                << " exceptions=" << serial.exceptions.size()
                << " timed_out=" << serial.timed_out << '\n';
        }
    std::cout << "G1_DECODE_SUMMARY submitted=" << run.submitted.size()
        << " successes=" << successes << " ordinary_failures=" << failures
        << " no_responder=" << missing.size() << " duplicate_callbacks=" << duplicates
        << " escaped_exceptions=" << run.exceptions.size()
        << " timed_out=" << run.timed_out
        << " scope=selected_private_primary_decode_inputs_not_a_crash_causality_claim\n";
    CHECK_FALSE(run.timed_out);
    CHECK(duplicates == 0);
    CHECK(run.submitted.size() == cases.size());
    CHECK(missing.empty());
    CHECK(run.exceptions.empty());
}
