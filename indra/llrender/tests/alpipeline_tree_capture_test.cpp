/**
 * @file alpipeline_tree_capture_test.cpp
 * @brief PPD-473: actual tree capture preserves ready-default semantics and rejection order.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace TreeCaptureContract
{
using U32 = std::uint32_t;
using F32 = float;
U32 gFrameCount = 473;
bool main_view = true;
bool matrix_ready = true;
bool diagnostic_throws = false;
unsigned matrix_calls = 0;
std::vector<std::string> diagnostics;

struct EndLog {};
constexpr EndLog end_log;
struct Diagnostic
{
    std::ostringstream stream;
    explicit Diagnostic(const char*)
    {
        if (diagnostic_throws) throw std::bad_alloc();
    }
    template<typename T> Diagnostic& operator<<(const T& value)
    { stream << value; return *this; }
    Diagnostic& operator<<(EndLog)
    { diagnostics.push_back(stream.str()); return *this; }
};
#define LL_WARNS(tag) Diagnostic(tag)
#define LL_ENDL end_log

struct ALMetalTexture { int generation = 1; };
struct ALMetalBuffer {};
struct LLImageGL
{
    std::shared_ptr<ALMetalTexture> native = std::make_shared<ALMetalTexture>();
    unsigned bind_updates = 0;
    ALMetalTexture* getMetalTexture() const { return native.get(); }
    std::shared_ptr<const ALMetalTexture> getMetalTextureLease() const { return native; }
    void forceUpdateBindStats() { ++bind_updates; }
};
struct LLViewerTexture
{
    LLImageGL* image = nullptr;
    LLImageGL* revived_image = nullptr;
    bool deleted = false;
    unsigned revival_calls = 0, revived_stats = 0, activation_calls = 0;
    LLImageGL* getGLTexture() const { return image; }
    void forceImmediateUpdate()
    {
        ++revival_calls;
        if (deleted) ++revived_stats;
        if (revived_image) image = revived_image;
    }
    void setActive() { ++activation_calls; deleted = false; }
};
template<typename T> struct Pointer
{
    T* value = nullptr;
    T* get() const { return value; }
};
struct LLViewerFetchedTexture
{
    inline static Pointer<LLViewerTexture> sDefaultImagep;
};
struct LLMatrix4a
{
    float values[16]{};
    LLMatrix4a() { values[0] = values[5] = values[10] = values[15] = 1.f; }
    const float* getF32ptr() const { return values; }
};
struct LLViewerRegion { LLMatrix4a mRenderMatrix; };
struct LLDrawable
{
    LLViewerRegion* region = nullptr;
    LLViewerRegion* getRegion() const { return region; }
};
struct LLVertexBuffer
{
    enum { TYPE_VERTEX, TYPE_NORMAL, TYPE_TEXCOORD0, TYPE_COLOR };
    U32 mask = 15, vertices = 3, indices = 3;
    bool publication_ready = true;
    unsigned prepares = 0;
    mutable unsigned vertex_reads = 0, index_reads = 0;
    mutable std::vector<unsigned> attribute_reads;
    std::shared_ptr<ALMetalBuffer> vertex = std::make_shared<ALMetalBuffer>();
    std::shared_ptr<ALMetalBuffer> index = std::make_shared<ALMetalBuffer>();
    bool hasDataType(unsigned type) const
    { attribute_reads.push_back(type); return (mask & (1u << type)) != 0; }
    bool prepareMetalBuffers(bool require_indices)
    { ++prepares; return publication_ready && vertex && (!require_indices || index); }
    U32 getNumVerts() const { ++vertex_reads; return vertices; }
    U32 getNumIndices() const { ++index_reads; return indices; }
    unsigned getIndexStride() const { return 2; }
    U32 getOffset(unsigned type) const { return type * 64; }
    ALMetalBuffer* getMetalVertexBuffer() const { return vertex.get(); }
    ALMetalBuffer* getMetalIndexBuffer() const { return index.get(); }
    std::shared_ptr<const ALMetalBuffer> getMetalVertexBufferLease() const { return vertex; }
    std::shared_ptr<const ALMetalBuffer> getMetalIndexBufferLease() const { return index; }
};
struct LLFace
{
    LLDrawable* drawable = nullptr;
    LLVertexBuffer* buffer = nullptr;
    LLDrawable* getDrawable() const { return drawable; }
    LLVertexBuffer* getVertexBuffer() const { return buffer; }
};
enum class ALMetalIndexType { uint16, uint32 };
struct ALMetalWorldSimpleBatch
{
    ALMetalBuffer* vertices = nullptr;
    ALMetalBuffer* indices = nullptr;
    U32 position_offset = 0, normal_offset = 0, texcoord_offset = 0, color_offset = 0;
    U32 vertex_count = 0, index_count = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    ALMetalTexture* diffuse_textures[1]{};
    U32 texture_count = 0;
    bool alpha_mask = false, use_vertex_color = true, alpha_cutoff_bias = false;
    float alpha_cutoff = 0.f;
    float modelview_projection[16]{}, modelview[16]{};
    float normal_matrix[3][4]{}, rigged_normal_matrix[3][4]{};
};
bool buildMetalWorldDrawMatrices(const LLMatrix4a*, LLMatrix4a&, LLMatrix4a&, LLMatrix4a&)
{ ++matrix_calls; return matrix_ready; }

// Controlled capture/resource boundaries. The production body chooses exactly
// which image reaches retainImage and whether a failure clears unrelated draws.
// Real clearDraws/WorldResourceLease bodies were separately exercised in the
// retained actual-owner preflight reproduction; this test does not claim GPU use.
struct LLPipeline
{
    struct MetalWorldSimpleCapture
    {
        bool frame_active = true, pass_active = true, complete = true;
        bool hero_probe_face_capture = false, reflection_probe_face_capture = false;
        U32 first_incomplete_source_line = 0;
        unsigned clears = 0;
        std::vector<ALMetalWorldSimpleBatch> batches;
        std::vector<int> source_references{1}, source_cohorts{1}, draw_order{1};
        std::vector<std::shared_ptr<const void>> owners;
        void markIncomplete(U32 line)
        {
            if (complete) first_incomplete_source_line = line;
            complete = false;
        }
        void clearDraws()
        { ++clears; batches.clear(); source_references.clear(); source_cohorts.clear(); draw_order.clear(); }
        void retainGeometry(const LLVertexBuffer& buffer)
        { owners.push_back(buffer.getMetalVertexBufferLease()); owners.push_back(buffer.getMetalIndexBufferLease()); }
        void retainImage(const LLImageGL* image)
        { if (image) owners.push_back(image->getMetalTextureLease()); }
    };
    std::unique_ptr<MetalWorldSimpleCapture> mMetalWorldSimpleCapture =
        std::make_unique<MetalWorldSimpleCapture>();
    static bool isMainWorldPass() { return main_view; }
    void captureMetalWorldTreeBatch(LLFace* face, LLViewerTexture* texture)
    {
        constexpr U32 tree_capture_source_line = 470473;
#include "../../newview/pipeline_tree_capture_native.inl"
    }
};
#undef LL_WARNS
#undef LL_ENDL

struct Context
{
    LLPipeline pipeline;
    LLViewerRegion region;
    LLDrawable drawable{&region};
    LLVertexBuffer buffer;
    LLFace face{&drawable, &buffer};
    LLImageGL image, default_image;
    LLViewerTexture texture{&image}, fallback{&default_image};
    Context()
    {
        LLViewerFetchedTexture::sDefaultImagep.value = &fallback;
        main_view = true; matrix_ready = true; diagnostic_throws = false;
        matrix_calls = 0; diagnostics.clear();
    }
    ~Context() { LLViewerFetchedTexture::sDefaultImagep.value = nullptr; }
    auto& capture() { return *pipeline.mMetalWorldSimpleCapture; }
    void draw() { pipeline.captureMetalWorldTreeBatch(&face, &texture); }
    void prior() { capture().batches.emplace_back(); }
    void checkRejected()
    {
        CHECK_FALSE(capture().complete);
        CHECK(capture().clears == 1);
        CHECK(capture().batches.empty());
        CHECK(capture().source_references.empty());
        CHECK(capture().source_cohorts.empty());
        CHECK(capture().draw_order.empty());
        CHECK(capture().first_incomplete_source_line == 470473);
    }
};
} // namespace TreeCaptureContract
} // namespace

namespace
{
namespace TreeCaptureContract
{
TEST_CASE("Native tree capture resolves cold images without discarding unrelated batches", "[pipeline][tree][ppd473]")
{
    Context c;
    c.prior();
    c.image.native.reset();
    c.texture.deleted = true;
    c.draw();
    REQUIRE(c.capture().complete);
    REQUIRE(c.capture().batches.size() == 2);
    const auto& batch = c.capture().batches.back();
    CHECK(batch.diffuse_textures[0] == c.default_image.native.get());
    CHECK(batch.alpha_mask);
    CHECK(batch.alpha_cutoff == .5f);
    CHECK_FALSE(batch.use_vertex_color);
    CHECK(c.buffer.prepares == 1);
    CHECK(c.texture.revival_calls == 1);
    CHECK(c.texture.revived_stats == 1); // force-before-active preserved DELETED
    CHECK(c.texture.activation_calls == 1);
    CHECK_FALSE(c.texture.deleted);
    CHECK(c.image.bind_updates == 1);
    CHECK(c.capture().clears == 0);
    std::weak_ptr<ALMetalTexture> lease = c.default_image.native;
    c.default_image.native.reset();
    CHECK_FALSE(lease.expired()); // resolved generation, not missing source, pinned
    c.capture().owners.clear();
    CHECK(lease.expired());
}

TEST_CASE("Native tree capture uses resident and revived resources without unnecessary activation", "[pipeline][tree][ppd473]")
{
    Context c;
    c.draw();
    REQUIRE(c.capture().complete);
    REQUIRE(c.capture().batches.size() == 1);
    CHECK(c.capture().batches[0].diffuse_textures[0] == c.image.native.get());
    CHECK(c.texture.revival_calls == 0);
    CHECK(c.texture.activation_calls == 0);
    CHECK(c.image.bind_updates == 0);
    CHECK(c.buffer.prepares == 1);

    Context revived;
    LLImageGL published;
    revived.image.native.reset();
    revived.texture.revived_image = &published;
    revived.draw();
    REQUIRE(revived.capture().complete);
    REQUIRE(revived.capture().batches.size() == 1);
    CHECK(revived.capture().batches[0].diffuse_textures[0] == published.native.get());
    CHECK(published.bind_updates == 1);
    CHECK(revived.default_image.bind_updates == 0);
}

TEST_CASE("Native tree capture keeps null and failed default inputs unrecoverable", "[pipeline][tree][ppd473]")
{
    for (unsigned mode = 0; mode != 5; ++mode)
    {
        INFO("mode=" << mode);
        Context c;
        c.prior();
        if (mode == 0) c.texture.image = nullptr;
        else if (mode == 1) { c.image.native.reset(); LLViewerFetchedTexture::sDefaultImagep.value = nullptr; }
        else if (mode == 2) { c.image.native.reset(); c.fallback.image = nullptr; }
        else if (mode == 3) { c.image.native.reset(); c.default_image.native.reset(); }
        if (mode == 4) c.pipeline.captureMetalWorldTreeBatch(&c.face, nullptr);
        else c.draw();
        c.checkRejected();
    }
}

TEST_CASE("Native tree capture reports the first failed prerequisite in original short circuit order", "[pipeline][tree][ppd473]")
{
    // Only this test uses view0. Each distinct first reason gets its own row.
    const std::array<const char*, 11> reasons{
        "missing_region", "missing_buffer", "missing_image", "missing_position",
        "missing_normal", "missing_texcoord", "missing_color", "geometry_not_published",
        "texture_not_published", "zero_vertices", "zero_indices"};
    for (unsigned failure = 0; failure != reasons.size(); ++failure)
    {
        INFO("failure=" << failure);
        Context c; main_view = false; c.prior();
        // Several invalid values are authored together. The earlier one must win.
        if (failure == 0) { c.drawable.region = nullptr; c.face.buffer = nullptr; }
        else if (failure == 1) { c.face.buffer = nullptr; c.texture.image = nullptr; }
        else if (failure == 2) { c.texture.image = nullptr; c.buffer.mask = 0; }
        else if (failure < 7) c.buffer.mask = (1u << (failure - 3)) - 1;
        else if (failure == 7) { c.buffer.publication_ready = false; c.image.native.reset(); LLViewerFetchedTexture::sDefaultImagep.value = nullptr; }
        else if (failure == 8) { c.image.native.reset(); LLViewerFetchedTexture::sDefaultImagep.value = nullptr; c.buffer.vertices = 0; }
        else if (failure == 9) { c.buffer.vertices = 0; c.buffer.indices = 0; }
        else c.buffer.indices = 0;
        c.draw();
        c.checkRejected();
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].find(std::string("reason=") + reasons[failure] + ", view=0") != std::string::npos);
        CHECK(c.buffer.prepares == (failure >= 7 ? 1u : 0u));
        const auto expected_attributes = failure < 3 ? 0u : failure < 7 ? failure - 2 : 4u;
        REQUIRE(c.buffer.attribute_reads.size() == expected_attributes);
        for (unsigned at = 0; at < expected_attributes; ++at) CHECK(c.buffer.attribute_reads[at] == at);
        CHECK(c.buffer.vertex_reads == (failure >= 9 ? 1u : 0u));
        CHECK(c.buffer.index_reads == (failure == 10 ? 1u : 0u));
        CHECK(matrix_calls == 0);
    }
    Context matrices; main_view = false; matrices.prior(); matrix_ready = false;
    matrices.draw(); matrices.checkRejected();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].find("reason=invalid_matrices, view=0") != std::string::npos);
    CHECK(matrices.buffer.prepares == 1);
    CHECK(matrix_calls == 1);
}

TEST_CASE("Native tree failure diagnostics remain bounded by reason and view", "[pipeline][tree][ppd473]")
{
    for (unsigned view : {2u, 3u})
    {
        Context c; c.drawable.region = nullptr;
        c.capture().hero_probe_face_capture = view == 2;
        c.capture().reflection_probe_face_capture = view == 3;
        c.draw();
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].find("missing_region, view=" + std::to_string(view)) != std::string::npos);
        c.capture().complete = true;
        c.draw();
        CHECK(diagnostics.size() == 1);
        CHECK(c.capture().clears == 2);
    }
    Context throwing;
    throwing.capture().hero_probe_face_capture = true;
    throwing.face.buffer = nullptr;
    diagnostic_throws = true;
    throwing.draw();
    throwing.checkRejected();
    CHECK(diagnostics.empty());
    diagnostic_throws = false;
}

TEST_CASE("Native tree capture does no work outside an active complete view", "[pipeline][tree][ppd473]")
{
    for (unsigned mode = 0; mode != 4; ++mode)
    {
        Context c; c.prior();
        c.image.native.reset();
        if (mode == 0) c.pipeline.mMetalWorldSimpleCapture.reset();
        else if (mode == 1) c.capture().frame_active = false;
        else if (mode == 2) c.capture().pass_active = false;
        else c.capture().complete = false;
        c.draw();
        CHECK(c.buffer.prepares == 0);
        CHECK(c.texture.revival_calls == 0);
        CHECK(c.texture.activation_calls == 0);
        CHECK(diagnostics.empty());
        if (mode) CHECK(c.capture().batches.size() == 1);
    }
}

} // namespace TreeCaptureContract
} // namespace
