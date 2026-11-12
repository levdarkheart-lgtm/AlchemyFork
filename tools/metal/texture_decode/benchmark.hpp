// Local CPU codec benchmark. Inputs and decoded pixels remain on this machine.
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace tdbench {
struct Decoded {
    uint32_t width = 0, height = 0, components = 0;
    std::vector<uint8_t> pixels; // top-down, interleaved, unsigned 8-bit
};
inline std::string quote(const std::string& s) {
    std::ostringstream o; o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else if (c < 32) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else o << c;
    }
    return o.str() + '"';
}
inline uint64_t hash(const std::vector<uint8_t>& pixels) {
    uint64_t h = 14695981039346656037ULL;
    for (auto p : pixels) { h ^= p; h *= 1099511628211ULL; }
    return h;
}
struct Case {
    std::string id, path, error, digest;
    int reduce = 0;
    std::vector<uint8_t> bytes;
    uint32_t width = 0, height = 0, components = 0;
    std::vector<int64_t> samples;
    unsigned failures = 0;
};
template<class Decode> int run(int argc, char** argv, const char* codec, Decode decode) {
    try {
        std::string manifest, output, dump;
        unsigned workers = 1, rounds = 3, warmup = 1;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            std::string value = argv[++i];
            if (arg == "--manifest") manifest = value;
            else if (arg == "--output") output = value;
            else if (arg == "--dump-dir") dump = value;
            else if (arg == "--workers") workers = std::stoul(value);
            else if (arg == "--rounds") rounds = std::stoul(value);
            else if (arg == "--warmup") warmup = std::stoul(value);
            else throw std::runtime_error("unknown option: " + arg);
        }
        if (manifest.empty() || output.empty() || !workers || workers > 32 || !rounds || rounds > 100 || !warmup || warmup > 100)
            throw std::runtime_error("invalid benchmark arguments");
        std::ifstream input(manifest);
        if (!input) throw std::runtime_error("cannot open manifest");
        std::vector<Case> cases;
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto a = line.find('\t'), b = line.rfind('\t');
            if (a == b || a == std::string::npos) throw std::runtime_error("invalid manifest row");
            Case c; c.id = line.substr(0,a); c.path = line.substr(a+1,b-a-1); c.reduce = std::stoi(line.substr(b+1));
            if (c.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
                throw std::runtime_error("unsafe case ID");
            std::ifstream f(c.path, std::ios::binary);
            if (!f) throw std::runtime_error("cannot read input case " + c.id);
            c.bytes.assign(std::istreambuf_iterator<char>(f), {});
            cases.push_back(std::move(c));
        }
        if (cases.empty()) throw std::runtime_error("empty manifest");
        if (!dump.empty()) std::filesystem::create_directories(dump);
        std::vector<double> wall_ms;
        std::vector<size_t> order(cases.size()); std::iota(order.begin(),order.end(),0);
        // Fresh workers each round; wall time includes worker creation/join. File reads,
        // hashes and pixel dumps are outside measured rounds. Codec-internal threads=1.
        for (unsigned round = 0; round < warmup + rounds; ++round) {
            std::mt19937 rng(0x5ec0d + round); // deterministic permutation for every codec
            std::shuffle(order.begin(),order.end(),rng);
            std::atomic<size_t> next{0};
            auto start = std::chrono::steady_clock::now();
            std::vector<std::thread> pool;
            for (unsigned w = 0; w < workers; ++w) pool.emplace_back([&] {
                for (;;) {
                    size_t slot = next.fetch_add(1); if (slot >= order.size()) break;
                    auto& c = cases[order[slot]];
                    auto t0 = std::chrono::steady_clock::now();
                    bool ok = false;
                    try {
                        auto image = decode(c.bytes,c.reduce);
                        if (!image.width || !image.height || !image.components ||
                            image.pixels.size() != size_t(image.width)*image.height*image.components)
                            throw std::runtime_error("invalid decoded layout");
                        // Validation is the first unmeasured warmup pass.
                        if (round == 0 && warmup) {
                            c.width=image.width; c.height=image.height; c.components=image.components;
                            std::ostringstream h; h << std::hex << hash(image.pixels); c.digest=h.str();
                            if (!dump.empty()) {
                                std::ofstream p(std::filesystem::path(dump)/(c.id+".raw"),std::ios::binary);
                                p.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size());
                                if (!p) throw std::runtime_error("pixel dump failed");
                            }
                        }
                        ok = true;
                    } catch (const std::exception& e) { c.error=e.what(); }
                    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count();
                    if (round >= warmup) { c.samples.push_back(ns); if (!ok) ++c.failures; }
                }
            });
            for (auto& t : pool) t.join();
            if (round >= warmup) wall_ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::ofstream out(output); if (!out) throw std::runtime_error("cannot open output");
        out << "{\"schema_version\":1,\"codec\":" << quote(codec) << ",\"workers\":" << workers
            << ",\"codec_threads\":1,\"rounds\":" << rounds << ",\"warmup_rounds\":" << warmup << ",\"wall_ms\":[";
        for (size_t i=0;i<wall_ms.size();++i) { if(i)out<<',';out<<std::setprecision(12)<<wall_ms[i]; }
        out << "],\"cases\":[";
        for (size_t i=0;i<cases.size();++i) {
            auto& c=cases[i]; if(i)out<<',';
            out << "{\"id\":"<<quote(c.id)<<",\"reduce\":"<<c.reduce<<",\"input_bytes\":"<<c.bytes.size()
                <<",\"width\":"<<c.width<<",\"height\":"<<c.height<<",\"components\":"<<c.components
                <<",\"fnv1a64\":"<<quote(c.digest)<<",\"error\":"<<quote(c.error)<<",\"failures\":"<<c.failures<<",\"samples_ns\":[";
            for(size_t j=0;j<c.samples.size();++j){if(j)out<<',';out<<c.samples[j];}out<<"]}";
        }
        out << "]}\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
} // namespace tdbench
