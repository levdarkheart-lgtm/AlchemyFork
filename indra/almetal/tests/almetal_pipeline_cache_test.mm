/**
 * @file almetal_pipeline_cache_test.mm
 * @brief Actual archive hits, native output, persistence and purge ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetalpipelinecache.h"
#include "../almetalpipeline.h"
#include <catch2/catch_test_macros.hpp>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
namespace fs = std::filesystem;
constexpr char shader[] = R"(
#include <metal_stdlib>
using namespace metal;
constant bool use_green [[function_constant(0)]];
struct V { float4 position [[position]]; };
vertex V cache_vertex(uint i [[vertex_id]]) {
    float2 p = float2((i << 1) & 2, i & 2);
    return {float4(p * 2.0 - 1.0, 0, 1)};
}
fragment float4 cache_fragment() {
    return use_green ? float4(0,1,0,1) : float4(1,0,0,1);
}
kernel void cache_compute(device uint* values [[buffer(0)]], uint i [[thread_position_in_grid]]) {
    values[i] += 7u;
}
)";
std::string alteredSource()
{
    std::string result(shader);
    result.replace(result.find("float4(1,0,0,1)"), 15, "float4(0,0,1,1)");
    result.replace(result.find("7u"), 2, "11u");
    return result;
}

struct Directory
{
    fs::path path = fs::temp_directory_path() /
        ("alchemy-native-pipeline-cache-" + std::string([NSUUID UUID].UUIDString.UTF8String));
    ~Directory()
    {
        std::error_code ec;
        if (fs::exists(path, ec))
            fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);
        fs::remove_all(path, ec);
    }
    ALMetalPipelineCachePolicy policy(bool enabled = true, bool read_only = false) const
    {
        return {enabled, read_only, path.string(), "native-cache-test-v1"};
    }
};

std::vector<char> fileBytes(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct GPU
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::unique_ptr<ALMetalShaderLibrary> library;
    bool render_instrumented = false, compute_instrumented = false;
    explicit GPU(const std::string& source = shader)
    {
        INFO(error); REQUIRE(device);
        REQUIRE_FALSE(device->pipelineCache().enabled());
        library = ALMetalShaderLibrary::compileSource(*device, source, "Archive test source", &error);
        INFO(error); REQUIRE(library);
    }
    void configure(const ALMetalPipelineCachePolicy& policy)
    {
        REQUIRE(device->pipelineCache().configure(policy, &error)); INFO(error);
    }
    std::unique_ptr<ALMetalRenderPipeline> renderPipeline(bool green = false)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "cache_vertex";
        descriptor.fragment_function = "cache_fragment";
        descriptor.fragment_bool_constants = {{0, green}};
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "Archive actual fragment specialization";
        auto pipeline = ALMetalRenderPipeline::create(*device, descriptor, &error);
        INFO(error); REQUIRE(pipeline);
        render_instrumented = ((__bridge id<MTLRenderPipelineState>)pipeline->nativeHandle())
            .shaderValidation == MTLShaderValidationEnabled;
        return pipeline;
    }
    std::unique_ptr<ALMetalRenderTarget> target()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = 8;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        auto output = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(output); return output;
    }
    std::unique_ptr<ALMetalCommandBuffer> encode(
        const ALMetalRenderPipeline& pipeline, const ALMetalRenderTarget& output)
    {
        auto command = device->makeCommandBuffer("Archive actual render output", &error); REQUIRE(command);
        auto pass = output.makeRenderPass({}, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Archive render", &error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(pipeline, &error));
        REQUIRE(encoder->setViewport({0,0,8,8,0,1}, &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error)); return command;
    }
    void pixels(const ALMetalRenderTarget& output, std::array<std::uint8_t,4> expected)
    {
        std::array<std::uint8_t,8*8*4> actual{};
        REQUIRE(output.colorTexture(0)->readback(*device, 0, 0, {}, actual.data(), 8*4, &error));
        for (unsigned pixel = 0; pixel < 64; ++pixel)
            for (unsigned channel = 0; channel < 4; ++channel)
                CHECK(actual[pixel * 4 + channel] == expected[channel]);
    }
    void render(bool green = false, bool blue = false)
    {
        auto pipeline = renderPipeline(green); auto output = target();
        auto command = encode(*pipeline, *output);
        REQUIRE(command->commitAndWait(&error));
        pixels(*output, green ? std::array<std::uint8_t,4>{0,255,0,255}
            : blue ? std::array<std::uint8_t,4>{0,0,255,255}
                   : std::array<std::uint8_t,4>{255,0,0,255});
    }
    void compute(unsigned add = 7, const std::string& label = "Archive actual compute output")
    {
        ALMetalComputePipelineDescriptor descriptor;
        descriptor.library = library.get(); descriptor.function = "cache_compute";
        descriptor.label = label;
        auto pipeline = ALMetalComputePipeline::create(*device, descriptor, &error); INFO(error); REQUIRE(pipeline);
        compute_instrumented = ((__bridge id<MTLComputePipelineState>)pipeline->nativeHandle())
            .shaderValidation == MTLShaderValidationEnabled;
        ALMetalBufferDescriptor bd; bd.length = 16 * sizeof(unsigned); bd.storage = ALMetalStorageMode::shared;
        auto buffer = ALMetalBuffer::create(*device, bd, &error); REQUIRE(buffer);
        std::array<unsigned,16> input{};
        for (unsigned i=0; i<input.size(); ++i) input[i]=i;
        REQUIRE(buffer->write(0, input.data(), sizeof(input), &error));
        auto command = device->makeCommandBuffer("Archive actual compute output", &error); REQUIRE(command);
        auto encoder = ALMetalComputeEncoder::begin(*command, "Archive compute", &error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error)); REQUIRE(encoder->setBuffer(0, *buffer, 0, &error));
        REQUIRE(encoder->dispatch(16,1,1,16,1,1,&error)); REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        const auto* actual = static_cast<const unsigned*>(buffer->mappedBytes()); REQUIRE(actual);
        for (unsigned i=0; i<input.size(); ++i) CHECK(actual[i] == i+add);
    }
};
}

TEST_CASE("Native pipeline archive proves disk hits and source/specialization misses with exact GPU output",
          "[almetal][pipeline-cache][pipeline-cache-disk]")
{
    @autoreleasepool
    {
        Directory directory;
        fs::path archive;
        {
            GPU f; f.configure(directory.policy());
            f.render(); f.compute();
            auto stats = f.device->pipelineCache().stats();
            std::cout << "Native archive cold receipt: render_hits=" << stats.render_hits
                << ", compute_hits=" << stats.compute_hits << ", misses=" << stats.misses
                << ", instrumented=" << f.render_instrumented << '/' << f.compute_instrumented
                << ", last_miss=" << stats.last_miss << '\n';
            CHECK(stats.misses == 2); CHECK(stats.render_hits == 0); CHECK(stats.compute_hits == 0);
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            archive = stats.file_path; REQUIRE(fs::is_regular_file(archive));
            REQUIRE(fs::is_regular_file(stats.compute_file_path));
            CHECK(fs::file_size(archive) > 0);
        }
        {
            GPU f; f.configure(directory.policy());
            CHECK(f.device->pipelineCache().stats().loaded_from_disk);
            f.render(); f.compute();
            auto stats = f.device->pipelineCache().stats();
            std::cout << "Native archive disk receipt: render_hits=" << stats.render_hits
                << ", compute_hits=" << stats.compute_hits << ", misses=" << stats.misses
                << ", instrumented=" << f.render_instrumented << '/' << f.compute_instrumented
                << ", last_miss=" << stats.last_miss << '\n';
            CHECK(stats.archives_loaded == 2); CHECK(stats.render_hits == 1);
            CHECK(stats.compute_hits == 1); CHECK(stats.misses == 0);
            f.render(true);
            CHECK(f.device->pipelineCache().stats().misses == 1);
            const bool saved = f.device->pipelineCache().flush(&f.error);
            INFO(f.error); REQUIRE(saved);
        }
        {
            GPU f; f.configure(directory.policy()); f.render(true);
            CHECK(f.device->pipelineCache().stats().render_hits == 1);
            CHECK(f.device->pipelineCache().stats().misses == 0);
        }
        {
            GPU f(alteredSource()); f.configure(directory.policy());
            f.render(false, true); f.compute(11);
            auto stats = f.device->pipelineCache().stats();
            CHECK(stats.render_hits == 0); CHECK(stats.compute_hits == 0); CHECK(stats.misses == 2);
        }
    }
}

TEST_CASE("Native pipeline archive disable, read-only and corrupt recovery preserve native output",
          "[almetal][pipeline-cache][pipeline-cache-disk]")
{
    @autoreleasepool
    {
        Directory directory;
        {
            GPU f;
            auto disabled = directory.policy(false); disabled.application_version.clear();
            f.configure(disabled); f.render(); f.compute();
            CHECK_FALSE(f.device->pipelineCache().enabled());
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            CHECK(f.device->pipelineCache().stats().file_path.empty());
            CHECK_FALSE(fs::exists(directory.path));
            f.configure(directory.policy(true, true)); f.render(); f.compute();
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            CHECK_FALSE(fs::exists(directory.path));
        }
        CHECK_FALSE(fs::exists(directory.path));
        fs::path archive, compute_archive;
        {
            GPU f; f.configure(directory.policy()); f.render(); f.compute();
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            archive = f.device->pipelineCache().stats().file_path;
            compute_archive = f.device->pipelineCache().stats().compute_file_path;
        }
        { std::ofstream file(archive, std::ios::binary | std::ios::trunc); file << "corrupt native archive"; }
        { std::ofstream file(compute_archive, std::ios::binary | std::ios::trunc); file << "corrupt native archive"; }
        {
            GPU f; f.configure(directory.policy());
            CHECK_FALSE(f.device->pipelineCache().stats().loaded_from_disk);
            CHECK(f.device->pipelineCache().stats().archive_errors > 0);
            f.render(); f.compute(); REQUIRE(f.device->pipelineCache().flush(&f.error));
        }
        const auto render_bytes = fileBytes(archive);
        const auto compute_bytes = fileBytes(compute_archive);
        {
            GPU f; f.configure(directory.policy(true, true)); f.render(); f.compute();
            CHECK(f.device->pipelineCache().stats().render_hits == 1);
            CHECK(f.device->pipelineCache().stats().compute_hits == 1);
            f.render(true);
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            CHECK(f.device->pipelineCache().stats().read_only);
            CHECK(f.device->pipelineCache().stats().serializations == 0);
        }
        CHECK(fileBytes(archive) == render_bytes);
        CHECK(fileBytes(compute_archive) == compute_bytes);
    }
}

TEST_CASE("Native pipeline archive purge invalidates dirty owners and preserves old unsubmitted GPU work",
          "[almetal][pipeline-cache][pipeline-cache-disk][lifetime]")
{
    @autoreleasepool
    {
        Directory directory;
        fs::path archive, compute_archive;
        {
            GPU f; f.configure(directory.policy());
            f.render(); f.compute(); REQUIRE(f.device->pipelineCache().flush(&f.error));
            archive = f.device->pipelineCache().stats().file_path;
            compute_archive = f.device->pipelineCache().stats().compute_file_path;
            auto pipeline = f.renderPipeline(true); auto output = f.target();
            auto command = f.encode(*pipeline, *output);
            pipeline.reset(); f.library.reset();
            const auto generation = f.device->pipelineCache().stats().generation;
            const fs::path keep = directory.path / "unrelated-file";
            { std::ofstream file(keep); file << "keep"; }
            const fs::path temporary = archive.string() + ".tmp-interrupted";
            { std::ofstream file(temporary); file << "partial"; }
            REQUIRE(ALMetalPipelineCache::clearDirectory(directory.path.string(), &f.error));
            CHECK_FALSE(f.device->pipelineCache().enabled());
            CHECK(f.device->pipelineCache().stats().generation > generation);
            CHECK_FALSE(fs::exists(archive)); CHECK_FALSE(fs::exists(compute_archive));
            CHECK_FALSE(fs::exists(temporary)); CHECK(fs::exists(keep));
            REQUIRE(f.device->pipelineCache().flush(&f.error)); CHECK_FALSE(fs::exists(archive));
            REQUIRE(command->commitAndWait(&f.error)); f.pixels(*output, {0,255,0,255});
        }
        CHECK_FALSE(fs::exists(archive)); // Device destructor cannot resurrect it.
        { std::ofstream file(archive); file << "old app-owned archive"; }
        std::string error;
        REQUIRE(ALMetalPipelineCache::clearDirectory(directory.path.string(), &error)); // No device exists.
        CHECK_FALSE(fs::exists(archive));
        CHECK_FALSE(ALMetalPipelineCache::clearDirectory("", &error));
        CHECK_FALSE(ALMetalPipelineCache::clearDirectory("/", &error));
        CHECK_FALSE(ALMetalPipelineCache::clearDirectory("relative/cache", &error));
        CHECK_FALSE(ALMetalPipelineCache::clearDirectory(directory.path.string()+"/../other", &error));
    }
}

TEST_CASE("Native pipeline archive failed write preserves the last valid file",
          "[almetal][pipeline-cache][pipeline-cache-disk][io]")
{
    @autoreleasepool
    {
        Directory directory;
        bool compute = false;
        SECTION("Render archive retry") {}
        SECTION("Compute archive retry") { compute = true; }
        GPU f; f.configure(directory.policy());
        if (compute) f.compute(); else f.render();
        REQUIRE(f.device->pipelineCache().flush(&f.error));
        const auto paths = f.device->pipelineCache().stats();
        const fs::path archive = compute ? paths.compute_file_path : paths.file_path;
        const auto original = fileBytes(archive); REQUIRE_FALSE(original.empty());
        if (compute)
        {
            f.library = ALMetalShaderLibrary::compileSource(*f.device, alteredSource(), "Changed compute source", &f.error);
            REQUIRE(f.library); f.compute(11);
        }
        else f.render(true);
        struct RestorePermissions
        {
            fs::path path;
            ~RestorePermissions() { std::error_code ec; fs::permissions(path, fs::perms::owner_all, ec); }
        } restore{directory.path};
        fs::permissions(directory.path, fs::perms::owner_read | fs::perms::owner_exec);
        CHECK_FALSE(f.device->pipelineCache().flush(&f.error));
        CHECK_FALSE(f.error.empty());
        CHECK(fileBytes(archive) == original);
        fs::permissions(directory.path, fs::perms::owner_all);
        const bool retry = f.device->pipelineCache().flush(&f.error);
        INFO(f.error); REQUIRE(retry);
        CHECK(f.device->pipelineCache().stats().serializations == 2);
        GPU reloaded(compute ? alteredSource() : shader); reloaded.configure(directory.policy());
        if (compute) reloaded.compute(11); else reloaded.render(true);
        const auto hits = reloaded.device->pipelineCache().stats();
        CHECK((compute ? hits.compute_hits : hits.render_hits) == 1);
        CHECK(reloaded.device->pipelineCache().stats().misses == 0);
    }
}

TEST_CASE("Native pipeline archive purge resolves aliases before invalidating every matching writer",
          "[almetal][pipeline-cache][pipeline-cache-disk][alias]")
{
    @autoreleasepool
    {
        Directory directory;
        fs::create_directories(directory.path / "physical");
        const auto physical = directory.path / "physical";
        const auto alias = directory.path / "alias";
        fs::create_directory_symlink(physical, alias);
        auto policy = directory.policy(); policy.directory = physical.string();
        fs::path archive;
        {
            GPU first; first.configure(policy); first.render();
            REQUIRE(first.device->pipelineCache().flush(&first.error));
            archive = first.device->pipelineCache().stats().file_path;
            first.render(true);
            GPU second; policy.directory = alias.string(); second.configure(policy); second.compute();
            REQUIRE(ALMetalPipelineCache::clearDirectory(alias.string(), &first.error));
            CHECK_FALSE(first.device->pipelineCache().enabled());
            CHECK_FALSE(second.device->pipelineCache().enabled());
            REQUIRE(first.device->pipelineCache().flush(&first.error));
            REQUIRE(second.device->pipelineCache().flush(&second.error));
            CHECK_FALSE(fs::exists(archive));

            // The same policy string must resolve again if its alias changes.
            first.configure(policy);
            const auto generation = first.device->pipelineCache().stats().generation;
            fs::create_directories(directory.path / "other");
            fs::remove(alias); fs::create_directory_symlink(directory.path / "other", alias);
            first.configure(policy);
            CHECK(first.device->pipelineCache().stats().generation > generation);
            CHECK(fs::path(first.device->pipelineCache().stats().file_path).parent_path() ==
                  fs::canonical(directory.path / "other"));
        }
        CHECK_FALSE(fs::exists(archive)); // Neither retired writer resurrects it.
        fs::remove(alias); fs::create_directory_symlink("/", alias);
        std::string error;
        CHECK_FALSE(ALMetalPipelineCache::clearDirectory(alias.string(), &error));
        fs::remove(alias); fs::create_directory_symlink(directory.path / "missing", alias);
        GPU f; policy.directory = (alias / "cache").string();
        CHECK_FALSE(f.device->pipelineCache().configure(policy, &error));
        CHECK_FALSE(f.device->pipelineCache().enabled());
        f.compute(); // Optional cache rejection still leaves the native factory usable.
    }
}

TEST_CASE("Native pipeline archive can persist newly seen pipelines after a prior successful flush",
          "[almetal][pipeline-cache][pipeline-cache-disk][incremental]")
{
    @autoreleasepool
    {
        Directory directory;
        GPU f; f.configure(directory.policy()); f.render();
        REQUIRE(f.device->pipelineCache().flush(&f.error));
        f.render(true);
        const bool second = f.device->pipelineCache().flush(&f.error);
        INFO(f.error); REQUIRE(second);
        f.compute();
        const bool third = f.device->pipelineCache().flush(&f.error);
        INFO(f.error); REQUIRE(third);
        CHECK(f.device->pipelineCache().stats().serializations == 3);
    }
}

TEST_CASE("Instrumented native pipelines report actual archive probe outcomes without suppressing validation",
          "[almetal][pipeline-cache][pipeline-cache-instrumented]")
{
    @autoreleasepool
    {
        Directory directory;
        {
            GPU f; f.configure(directory.policy()); f.render(); f.compute();
            if (!f.render_instrumented || !f.compute_instrumented)
                SKIP("Requires actual native shader-validation pipelines; run with MTL_SHADER_VALIDATION=1");
            REQUIRE(f.device->pipelineCache().flush(&f.error));
            CHECK(f.device->pipelineCache().stats().shader_validation_bypasses == 2);
            CHECK(f.device->pipelineCache().stats().serializations == 0);
            CHECK_FALSE(fs::exists(directory.path));
        }
        GPU f;
        fs::create_directories(directory.path);
        const fs::path normal_archive = directory.path / ("alchemy-metal-pipeline-v1-" +
            std::to_string(f.device->info().registry_id) + "-default-preserve.metalarc");
        { std::ofstream file(normal_archive); file << "preserve default namespace"; }
        f.configure(directory.policy());
        CHECK(fs::exists(normal_archive));
        CHECK_FALSE(f.device->pipelineCache().stats().loaded_from_disk);
        f.render(); f.compute();
        CHECK(f.render_instrumented); CHECK(f.compute_instrumented);
        const auto stats = f.device->pipelineCache().stats();
        // Record the runtime capability, not an assumed GPU-family rule.
        CHECK(stats.render_hits == 0); CHECK(stats.compute_hits == 0);
        CHECK(stats.shader_validation_bypasses == 2);
        CHECK(stats.misses <= 2);
        if (stats.misses) CHECK_FALSE(stats.last_miss.empty());
        std::cout << "Instrumented native archive receipt: render_validation=" << f.render_instrumented
            << ", compute_validation=" << f.compute_instrumented << ", render_hits=" << stats.render_hits
            << ", compute_hits=" << stats.compute_hits << ", misses=" << stats.misses
            << ", validation_bypasses=" << stats.shader_validation_bypasses
            << ", last_miss=" << stats.last_miss << '\n';
        REQUIRE(f.device->pipelineCache().flush(&f.error));
        CHECK_FALSE(fs::exists(stats.file_path));
        auto pipeline = f.renderPipeline(true); auto output = f.target();
        auto command = f.encode(*pipeline, *output);
        pipeline.reset(); f.library.reset();
        REQUIRE(ALMetalPipelineCache::clearDirectory(directory.path.string(), &f.error));
        CHECK_FALSE(f.device->pipelineCache().enabled());
        REQUIRE(f.device->pipelineCache().flush(&f.error));
        CHECK_FALSE(fs::exists(stats.file_path));
        REQUIRE(command->commitAndWait(&f.error)); f.pixels(*output, {0,255,0,255});
    }
}

TEST_CASE("Native compute archive preserves ordinary creation-label shader validation selection",
          "[almetal][pipeline-cache][pipeline-cache-selective]")
{
    @autoreleasepool
    {
        const char* selection = std::getenv("MTL_SHADER_VALIDATION_ENABLE_PIPELINES");
        const char* defaults = std::getenv("MTL_SHADER_VALIDATION_DEFAULT_STATE");
        if (!selection || std::string(selection) != "cache_selective_compute" ||
            !defaults || std::string(defaults) != "none")
            SKIP("Requires MTL_SHADER_VALIDATION=1 DEFAULT_STATE=none ENABLE_PIPELINES=cache_selective_compute");
        Directory directory;
        GPU f;
        id<MTLLibrary> library = (__bridge id<MTLLibrary>)f.library->nativeHandle();
        MTLComputePipelineDescriptor* labeled = [[MTLComputePipelineDescriptor alloc] init];
        labeled.computeFunction = [library newFunctionWithName:@"cache_compute"];
        labeled.label = @"cache_selective_compute";
        NSError* error = nil;
        id<MTLComputePipelineState> control = [(__bridge id<MTLDevice>)f.device->nativeHandle()
            newComputePipelineStateWithDescriptor:labeled options:MTLPipelineOptionNone reflection:nil error:&error];
        INFO((error ? error.localizedDescription.UTF8String : "")); REQUIRE(control);
        REQUIRE(control.shaderValidation == MTLShaderValidationEnabled);
        f.compute(7, "cache_selective_compute");
        CHECK_FALSE(f.compute_instrumented); // Ordinary factory ignores this display label.
        f.configure(directory.policy()); f.compute(7, "cache_selective_compute");
        CHECK_FALSE(f.compute_instrumented);
        CHECK(f.device->pipelineCache().stats().shader_validation_bypasses == 0);
        REQUIRE(f.device->pipelineCache().flush(&f.error));
        GPU reloaded; reloaded.configure(directory.policy());
        reloaded.compute(7, "cache_selective_compute");
        CHECK_FALSE(reloaded.compute_instrumented);
        const auto outcome = reloaded.device->pipelineCache().stats();
        // Even a selectively uninstrumented PSO can miss while the global
        // validation layer is loaded. Preserve selection, report the real
        // outcome, and leave mandatory archive-hit checks in the API-only run.
        CHECK(outcome.compute_hits + outcome.misses == 1);
        if (outcome.misses) CHECK_FALSE(outcome.last_miss.empty());
        std::cout << "Selective native archive receipt: raw_labeled_validation=1, ordinary_validation="
            << f.compute_instrumented << ", cached_validation=" << reloaded.compute_instrumented
            << ", disk_hits=" << outcome.compute_hits << ", misses=" << outcome.misses
            << ", last_miss=" << outcome.last_miss << '\n';
    }
}
