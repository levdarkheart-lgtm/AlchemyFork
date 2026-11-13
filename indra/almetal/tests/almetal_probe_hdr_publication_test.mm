/** Production ordinary-probe producer, shared manager policy and publication. */
#include "../almetalworld.h"
#include "../almetalprobepublication.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
namespace
{
constexpr unsigned SIDE = 16, LOD = 3, MIPS = 4;
struct Fixture
{
    std::string                                  error;
    std::unique_ptr<ALMetalDevice>               device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error);
        REQUIRE(renderer);
    }
    ~Fixture()
    {
        if (device)
            CHECK(device->waitUntilIdle(&error));
    }
    std::unique_ptr<ALMetalRenderTarget> face()
    {
        ALMetalRenderTargetDescriptor d;
        d.width = d.height = SIDE * 2;
        d.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto t             = ALMetalRenderTarget::create(*device, d, &error);
        REQUIRE(t);
        std::vector<std::array<__fp16, 4>> pixels(SIDE * SIDE * 4, { .125f, .25f, .5f, 0.f });
        auto                               cmd = device->makeCommandBuffer("Author completed probe face", &error);
        REQUIRE(cmd);
        REQUIRE(const_cast<ALMetalTexture*>(t->colorTexture(0))->upload(*cmd, 0, 0, {}, pixels.data(), SIDE * 2 * 8, &error));
        REQUIRE(cmd->commitAndWait(&error));
        return t;
    }
    std::unique_ptr<ALMetalTexture> texture(ALMetalTextureDescriptor d)
    {
        auto t = ALMetalTexture::create(*device, d, &error);
        INFO(error);
        REQUIRE(t);
        return t;
    }
    std::vector<unsigned char> read(const ALMetalTexture& t, unsigned mip = 0, unsigned slice = 0)
    {
        const auto                 row = t.width(mip) * t.bytesPerPixel();
        std::vector<unsigned char> b(row * t.height(mip));
        REQUIRE(t.readback(*device, mip, slice, {}, b.data(), row, &error));
        return b;
    }
    template<class T>
    std::unique_ptr<ALMetalBuffer> buffer(const T& data)
    {
        ALMetalBufferDescriptor d;
        d.length  = sizeof(data);
        d.storage = ALMetalStorageMode::shared;
        auto b    = ALMetalBuffer::create(*device, d, &error);
        REQUIRE(b);
        REQUIRE(b->write(0, &data, sizeof(data), &error));
        return b;
    }
    void consume(ALMetalTexture& radiance, ALMetalTexture& sh, bool hdr, bool bootstrap = false)
    {
        constexpr unsigned S = 8;
        struct Streams
        {
            float         position[4][4]{ { -1, -1, .25f, 0 }, { 1, -1, .25f, 0 }, { -1, 1, .25f, 0 }, { 1, 1, .25f, 0 } };
            float         normal[4][4]{ { 0, 0, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 0 } };
            float         uv[4][2]{ { .5f, .5f }, { .5f, .5f }, { .5f, .5f }, { .5f, .5f } };
            unsigned char color[4][4]{ { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 } };
        } data;
        const std::array<std::uint16_t, 6> index{ 0, 1, 2, 2, 1, 3 };
        auto                               vertices = buffer(data);
        auto                               indices  = buffer(index);
        ALMetalTextureDescriptor           d;
        d.format   = ALMetalPixelFormat::rgba8_unorm_srgb;
        auto white = texture(d);
        d.width = d.height = S;
        d.format           = ALMetalPixelFormat::rg8_unorm;
        auto light         = texture(d);
        auto upload        = device->makeCommandBuffer("Author probe consumer geometry inputs", &error);
        REQUIRE(upload);
        const std::array<unsigned char, 4> w{ 255, 255, 255, 255 };
        REQUIRE(white->upload(*upload, 0, 0, {}, w.data(), 4, &error));
        std::array<unsigned char, S * S * 2> l;
        l.fill(255);
        REQUIRE(light->upload(*upload, 0, 0, {}, l.data(), S * 2, &error));
        REQUIRE(upload->commitAndWait(&error));
        ALMetalWorldSimpleBatch batch;
        batch.vertices            = vertices.get();
        batch.indices             = indices.get();
        batch.vertex_count        = 4;
        batch.index_count         = 6;
        batch.normal_offset       = offsetof(Streams, normal);
        batch.texcoord_offset     = offsetof(Streams, uv);
        batch.color_offset        = offsetof(Streams, color);
        batch.double_sided        = true;
        batch.texture_count       = 1;
        batch.diffuse_textures[0] = white.get();
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1;
            batch.modelview[axis * 4 + axis]            = 1;
        }
        for (unsigned axis = 0; axis < 3; ++axis)
            batch.normal_matrix[axis][axis] = 1;
        ALMetalWorldGBufferScene geometry;
        geometry.width = geometry.height = S;
        geometry.hdr                     = hdr;
        geometry.normal_hdr              = true;
        geometry.batches                 = &batch;
        geometry.batch_count             = 1;
        auto gbuffer                     = renderer->render(geometry, &error);
        INFO(error);
        REQUIRE(gbuffer);
        ALMetalWorldReflectionProbes probes;
        probes.radiance_texture           = &radiance;
        probes.irradiance_texture         = &sh;
        probes.constants.config[0]        = bootstrap ? 0 : 1;
        probes.constants.config[1]        = LOD;
        probes.constants.ref_index[0][0]  = bootstrap ? 0 : 1;
        probes.constants.ref_index[0][1]  = -1;
        probes.constants.ref_sphere[0][3] = 100;
        probes.constants.ref_params[0][0] = 1;
        probes.constants.ref_params[0][1] = 1;
        probes.constants.ref_params[0][2] = 1;
        probes.constants.ref_params[0][3] = -100;
        ALMetalWorldBaseLightingScene scene;
        scene.gbuffer                          = gbuffer.get();
        scene.light_map                        = light.get();
        scene.reflection_probes                = &probes;
        scene.hdr                              = hdr;
        scene.constants.lighting[0]            = 1;
        scene.constants.light_direction[2]     = 1;
        scene.constants.atmos_sunlit_linear[0] = .125f;
        scene.constants.atmos_sunlit_linear[1] = .25f;
        scene.constants.atmos_sunlit_linear[2] = .5f;
        scene.constants.surface[0]             = 1;
        scene.constants.surface[1]             = 1;
        for (unsigned axis = 0; axis < 4; ++axis)
            scene.constants.inverse_projection[axis * 4 + axis] = 1;
        auto lit = renderer->renderBaseLighting(scene, &error);
        std::cout << "PROBE_CONSUMER hdr=" << hdr << " bootstrap=" << bootstrap << " accepted=" << bool(lit) << " error=" << error << '\n';
        INFO(error);
        CHECK(lit);
        if (!lit)
            return;
        std::array<std::uint16_t, S * S * 4> out{};
        REQUIRE(lit->colorTexture(0)->readback(*device, 0, 0, {}, out.data(), S * 8, &error));
        std::array<double, 3> mean{};
        bool                  nonzero = false;
        for (unsigned i = 0; i < S * S; ++i)
            for (unsigned c = 0; c < 3; ++c)
            {
                __fp16 value;
                std::memcpy(&value, &out[i * 4 + c], 2);
                CHECK(std::isfinite(float(value)));
                mean[c] += float(value) / (S * S);
                if (bootstrap)
                    CHECK(std::abs(float(value) - std::array<float, 3>{ .125f, .25f, .5f }[c]) < .002f);
                nonzero |= value > 0;
            }
        CHECK(nonzero);
        std::cout << "PROBE_PIXELS hdr=" << hdr << " bootstrap=" << bootstrap << " mean=" << mean[0] << ',' << mean[1] << ',' << mean[2]
                  << '\n';
        if (!bootstrap)
            for (unsigned c = 0; c < 3; ++c)
            {
                CAPTURE(c, mean[c]);
                CHECK(mean[c] > std::array<float, 3>{ .125f, .25f, .5f }[c] + .01f);
            }
    }
    void retry(const ALMetalRenderTarget& face, bool hdr, std::unique_ptr<ALMetalTexture>& source,
               std::unique_ptr<ALMetalTexture>& filtered, ALMetalTexture& published, unsigned& valid)
    {
        // Preserve the exact five-face admission prefix around a REAL producer
        // update and a REAL rejected blit, then retry that same final face.
        valid = 0x1f;
        {
            ALMetalProbeFacePublication publication(valid);
            REQUIRE(renderer->updateReflectionProbeFace(face, 5, SIDE, LOD, hdr, source, valid, &error));
            CHECK(valid == 0x3f);
            REQUIRE(renderer->filterReflectionProbeRadiance(*source, valid, LOD, filtered, &error));
            auto d            = published.descriptor();
            d.format          = ALMetalPixelFormat::rgba16_float;
            auto incompatible = texture(d);
            auto cmd          = device->makeCommandBuffer("Forced final-face publication failure", &error);
            REQUIRE(cmd);
            CHECK_FALSE(incompatible->copyFrom(*cmd, *filtered, {}, &error));
        }
        CHECK(valid == 0x1f);
        error.clear();
        {
            ALMetalProbeFacePublication publication(valid);
            REQUIRE(renderer->updateReflectionProbeFace(face, 5, SIDE, LOD, hdr, source, valid, &error));
            REQUIRE(renderer->filterReflectionProbeRadiance(*source, valid, LOD, filtered, &error));
            auto cmd = device->makeCommandBuffer("Retry final-face publication", &error);
            REQUIRE(cmd);
            for (unsigned mip = 0; mip < MIPS; ++mip)
                for (unsigned i = 0; i < 6; ++i)
                {
                    ALMetalTextureCopy c;
                    c.source_mip_level = c.destination_mip_level = mip;
                    c.source_slice                               = i;
                    c.destination_slice                          = 6 + i;
                    REQUIRE(published.copyFrom(*cmd, *filtered, c, &error));
                }
            REQUIRE(cmd->commitAndWait(&error));
            publication.commit();
        }
        CHECK(valid == 0x3f);
    }
};
} // namespace
TEST_CASE("Ordinary probe radiance and SH publish into the actual manager format policy", "[probe-hdr-publication]")
{
    @autoreleasepool
    {
        Fixture f;
        for (bool hdr : { false, true })
        {
            CAPTURE(hdr);
            auto                            face = f.face();
            std::unique_ptr<ALMetalTexture> source;
            unsigned                        valid = 0;
            for (unsigned i = 0; i < 6; ++i)
            {
                REQUIRE(f.renderer->updateReflectionProbeFace(*face, i, SIDE, LOD, hdr, source, valid, &f.error));
                CHECK(valid == ((1u << (i + 1)) - 1u));
            }
            REQUIRE(source);
            REQUIRE(valid == 0x3f);
            ALMetalTextureDescriptor shdesc;
            shdesc.format                                   = ALMetalPixelFormat::rgba32_uint;
            shdesc.width                                    = 4;
            shdesc.height                                   = 1;
            shdesc.usage                                    = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::shader_write;
            auto                                  staged_sh = f.texture(shdesc);
            std::unique_ptr<ALMetalBuffer>        packed;
            std::unique_ptr<ALMetalCommandBuffer> pending;
            REQUIRE(f.renderer->projectReflectionProbeSH(*source, valid, 1, 8, *staged_sh, 0, packed, pending, &f.error));
            REQUIRE(pending);
            REQUIRE(pending->waitUntilCompleted(&f.error));
            std::array<unsigned, 16> shwords{};
            REQUIRE(packed->read(0, shwords.data(), sizeof(shwords), &f.error));
            CHECK(std::any_of(shwords.begin(), shwords.end(), [](unsigned x) { return x != 0; }));
            std::unique_ptr<ALMetalTexture> filtered;
            REQUIRE(f.renderer->filterReflectionProbeRadiance(*source, valid, LOD, filtered, &f.error));
            REQUIRE(filtered);
            // This helper is consumed by ALL live manager array/bootstrap/reset
            // policy sites. No independently copied format constant is an oracle.
            ALMetalTextureDescriptor desc;
            desc.type   = ALMetalTextureType::texture_cube_array;
            desc.format = alMetalReflectionProbeRadianceFormat(hdr);
            desc.width = desc.height = SIDE;
            desc.mipmap_level_count  = MIPS;
            desc.array_length        = 2;
            desc.usage               = ALMetalTextureUsage::shader_read;
            auto published           = f.texture(desc);
            shdesc.height            = 2;
            shdesc.usage             = ALMetalTextureUsage::shader_read;
            auto published_sh        = f.texture(shdesc);
            auto clear               = f.device->makeCommandBuffer("Initialize coherent bootstrap pair", &f.error);
            REQUIRE(clear);
            for (unsigned mip = 0; mip < MIPS; ++mip)
            {
                unsigned                   n = SIDE >> mip;
                std::vector<unsigned char> zero(n * n * published->bytesPerPixel(), 0);
                for (unsigned slice = 0; slice < 12; ++slice)
                    REQUIRE(published->upload(*clear, mip, slice, {}, zero.data(), n * published->bytesPerPixel(), &f.error));
            }
            std::array<unsigned, 32> zero_sh{};
            REQUIRE(published_sh->upload(*clear, 0, 0, {}, zero_sh.data(), 64, &f.error));
            REQUIRE(clear->commitAndWait(&f.error));
            f.consume(*published, *published_sh, hdr, true);
            std::cout << "PROBE_FORMAT hdr=" << hdr << " source=" << int(source->descriptor().format)
                      << " filtered=" << int(filtered->descriptor().format) << " published=" << int(published->descriptor().format)
                      << " validFaces=" << valid << '\n';
            // The same mip/face/slot blits and conditional SH copy used by the
            // manager transaction must succeed before that pair can be published.
            auto copy = f.device->makeCommandBuffer("Publish coherent radiance and SH", &f.error);
            REQUIRE(copy);
            bool copied = true;
            for (unsigned mip = 0; copied && mip < MIPS; ++mip)
                for (unsigned i = 0; copied && i < 6; ++i)
                {
                    ALMetalTextureCopy c;
                    c.source_mip_level = c.destination_mip_level = mip;
                    c.source_slice                               = i;
                    c.destination_slice                          = 6 + i;
                    copied                                       = published->copyFrom(*copy, *filtered, c, &f.error);
                }
            if (copied)
            {
                ALMetalTextureCopy c;
                c.source_region = { 0, 0, 4, 1 };
                c.destination_y = 1;
                copied          = published_sh->copyFrom(*copy, *staged_sh, c, &f.error);
            }
            std::cout << "PROBE_COPY hdr=" << hdr << " copied=" << copied << " error=" << f.error << '\n';
            INFO(f.error);
            CHECK(copied);
            if (!copied)
            {
                const auto untouched = f.read(*published);
                CHECK(std::all_of(untouched.begin(), untouched.end(), [](auto x) { return x == 0; }));
                continue;
            }
            REQUIRE(copy->commitAndWait(&f.error));
            for (unsigned mip = 0; mip < MIPS; ++mip)
                for (unsigned i = 0; i < 6; ++i)
                {
                    CHECK(f.read(*published, mip, 6 + i) == f.read(*filtered, mip, i));
                    auto untouched = f.read(*published, mip, i);
                    CHECK(std::all_of(untouched.begin(), untouched.end(), [](auto x) { return x == 0; }));
                }
            const auto actual = f.read(*published_sh);
            REQUIRE(actual.size() == 128);
            CHECK(std::all_of(actual.begin(), actual.begin() + 64, [](auto x) { return x == 0; }));
            CHECK(std::memcmp(actual.data() + 64, shwords.data(), 64) == 0);
            f.consume(*published, *published_sh, hdr);
            f.retry(*face, hdr, source, filtered, *published, valid);
        }
    }
}
