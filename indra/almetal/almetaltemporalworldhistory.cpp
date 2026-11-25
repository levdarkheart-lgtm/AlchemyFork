/**
 * @file almetaltemporalworldhistory.cpp
 * @brief Bounded transactional main-world pose history, independent of simulation updates.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetaltemporalworldhistory.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <unordered_map>
#include <utility>

namespace
{
using Pose = ALMetalWorldTemporalPreviousDraw;
using Input = ALMetalTemporalWorldHistory::FrameInput;

bool fail(std::string* error, const char* text) noexcept
{
    if (error) { try { *error = text; } catch (...) {} }
    return false;
}

template<std::size_t N> bool finite(const float (&values)[N]) noexcept
{ return std::all_of(values, values + N, [](float x) { return std::isfinite(x); }); }

std::array<float,16> identity() noexcept
{
    std::array<float,16> out{};
    out[0] = out[5] = out[10] = out[15] = 1.f;
    return out;
}

// Invert in double precision, then multiply in the original column-major clip
// convention. No GL/Metal coordinate conversion belongs in this CPU matrix.
bool previousClip(const float* current, const float* previous, std::array<float,16>& out) noexcept
{
    double row[4][8]{};
    for (unsigned r = 0; r != 4; ++r)
        for (unsigned c = 0; c != 4; ++c)
        { row[r][c] = current[c * 4 + r]; row[r][c + 4] = r == c ? 1. : 0.; }
    for (unsigned c = 0; c != 4; ++c)
    {
        unsigned pivot = c;
        for (unsigned r = c + 1; r != 4; ++r)
            if (std::abs(row[r][c]) > std::abs(row[pivot][c])) pivot = r;
        if (std::abs(row[pivot][c]) < 1.e-15) return false;
        for (unsigned k = 0; k != 8; ++k) std::swap(row[c][k], row[pivot][k]);
        const double divisor = row[c][c];
        for (double& value : row[c]) value /= divisor;
        for (unsigned r = 0; r != 4; ++r)
            if (r != c)
            {
                const double amount = row[r][c];
                for (unsigned k = 0; k != 8; ++k) row[r][k] -= amount * row[c][k];
            }
    }
    for (unsigned c = 0; c != 4; ++c)
        for (unsigned r = 0; r != 4; ++r)
        {
            double value = 0.;
            for (unsigned k = 0; k != 4; ++k) value += previous[k * 4 + r] * row[k][c + 4];
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) return false;
            out[c * 4 + r] = static_cast<float>(value);
        }
    return true;
}

float halton(std::uint32_t sample, std::uint32_t base) noexcept
{
    float value = 0.f, fraction = 1.f;
    while (sample) { fraction /= base; value += fraction * (sample % base); sample /= base; }
    return value - .5f;
}

bool compatible(const Input& a, const Input& b) noexcept
{
    return a.input_width == b.input_width && a.input_height == b.input_height &&
        a.output_width == b.output_width && a.output_height == b.output_height &&
        a.mode == b.mode && a.reverse_z == b.reverse_z && a.origin == b.origin;
}

// An explicit producer instance+generation supplements the exact native
// geometry generation/layout/range key. Transient CPU capture addresses never
// identify an instance; equivalent raw avatar captures move between vectors.
// Reused slots and topology changes therefore cannot borrow an unrelated pose.
struct Key
{
    std::array<std::uint64_t, 33> values{};
    bool operator==(const Key&) const = default;
};
struct Hash
{
    std::size_t operator()(const Key& key) const noexcept
    {
        std::size_t hash = 0x9e3779b9u;
        for (const auto value : key.values)
            hash ^= std::hash<std::uint64_t>{}(value) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        return hash;
    }
};

Key key(const ALMetalWorldDraw& draw, bool forward)
{
    const auto& b = draw.source();
    Key k;
    auto& v = k.values;
    v[0] = forward;
    v[1] = b.temporal_instance_id;
    v[2] = b.temporal_generation;
    if (b.vertices)
    { v[4] = b.vertices->resourceView().identity(); v[5] = b.vertices->resourceView().generation(); }
    if (b.indices)
    { v[6] = b.indices->resourceView().identity(); v[7] = b.indices->resourceView().generation(); }
    v[8] = b.position_offset; v[9] = b.normal_offset; v[10] = b.texcoord_offset;
    v[11] = b.texcoord1_offset; v[12] = b.texcoord2_offset;
    v[13] = b.color_offset; v[14] = b.weight_offset; v[15] = b.cloth_offset;
    v[16] = b.vertex_count; v[17] = b.index_offset; v[18] = b.index_count;
    v[19] = static_cast<std::uint64_t>(b.index_type);
    v[20] = b.material_slot_count; v[21] = b.material_alpha_mode; v[22] = b.pbr_slot_count;
    v[23] = std::bit_cast<std::uint32_t>(b.alpha_cutoff);
    v[24] = static_cast<std::uint64_t>(b.avatar_cull_mode);
    v[25] = (std::uint64_t(b.avatar_skinning) << 0u) | (std::uint64_t(b.avatar_rigid) << 1u) |
        (std::uint64_t(b.avatar_cloth) << 2u) | (std::uint64_t(b.alpha_mask) << 3u) |
        (std::uint64_t(b.avatar_alpha) << 4u) | (std::uint64_t(b.avatar_depth_write) << 5u) |
        (std::uint64_t(b.avatar_color_write) << 6u) | (std::uint64_t(b.post_water_depth_only) << 7u) |
        (std::uint64_t(b.pre_water_alpha) << 8u) | (std::uint64_t(b.post_water_alpha) << 9u) |
        (std::uint64_t(b.pre_water_particle) << 10u) | (std::uint64_t(b.terrain) << 11u);
    v[26] = b.tangent_offset; v[27] = b.emissive_offset;
    v[28] = draw.skinJointCount(); v[29] = draw.skinPaletteFloatCount();
    // Source-publication identity distinguishes different logical sources over
    // a shared geometry range without retaining the publication itself.
    if (!v[1]) v[30] = reinterpret_cast<std::uintptr_t>(b.native_resource_source.get());
    v[31] = reinterpret_cast<std::uintptr_t>(b.native_material_source.get());
    // glTF UV animation can update per-material transforms without changing
    // the legacy batch texture matrix. Reject history when that coverage input
    // changes, including raw authored (not yet published) material tables.
    std::uint64_t uv_revision = 1469598103934665603ull;
    const auto mix = [&](float value)
    { uv_revision = (uv_revision ^ std::bit_cast<std::uint32_t>(value)) * 1099511628211ull; };
    for (std::size_t slot = 0; slot < std::min<std::size_t>(b.pbr_slot_count, b.max_material_slots); ++slot)
    {
        const auto& material = b.pbrSlots()[slot];
        for (const auto& transform : material.texture_transform)
            for (float value : transform) mix(value);
        mix(material.alpha_cutoff); mix(material.base_color_factor[3]);
    }
    for (std::size_t slot = 0; slot < std::min<std::size_t>(b.material_slot_count, b.max_material_slots); ++slot)
        mix(b.materialSlots()[slot].alpha_cutoff);
    v[32] = uv_revision;
    return k;
}

bool snapshot(const ALMetalWorldDraw& draw, Pose& pose, std::string* error)
{
    const auto& b = draw.source();
    std::copy_n(draw.modelviewProjection(), 16, pose.modelview_projection);
    std::copy_n(draw.textureMatrix(), 16, pose.texture_matrix);
    std::copy_n(b.avatar_wind, 4, pose.avatar_wind);
    std::copy_n(b.avatar_sin_wave, 4, pose.avatar_sin_wave);
    std::copy_n(b.avatar_gravity, 4, pose.avatar_gravity);
    std::copy_n(b.avatar_cloth_depth, 4, pose.avatar_cloth_depth);
    pose.skin_joint_count = draw.skinJointCount();
    const auto palette_size = draw.skinPaletteFloatCount();
    if (palette_size > 4096 || (palette_size && !draw.skinPalette()) ||
        (!palette_size && pose.skin_joint_count))
        return fail(error, "Temporal previous-pose palette is missing or exceeds the bounded pose layout");
    if (palette_size) pose.skin_palette.assign(draw.skinPalette(), draw.skinPalette() + palette_size);
    if (!finite(pose.modelview_projection) || !finite(pose.texture_matrix) ||
        !finite(pose.avatar_wind) || !finite(pose.avatar_sin_wave) ||
        !finite(pose.avatar_gravity) || !finite(pose.avatar_cloth_depth) ||
        !std::all_of(pose.skin_palette.begin(), pose.skin_palette.end(), [](float x) { return std::isfinite(x); }))
        return fail(error, "Temporal instance matrices or deformation palette contain non-finite values");
    pose.valid = true;
    // Deforming/animated UVs cannot be represented by a position vector alone.
    // The renderer additionally marks blended/special content reactive.
    pose.reactive = b.pre_water_particle || b.impostor || b.water_surface || b.water_haze;
    return true;
}
}

void alMetalApplyTemporalJitter(float matrix[16], float x, float y,
    std::uint32_t width, std::uint32_t height) noexcept
{
    if (!matrix || !width || !height || !std::isfinite(x) || !std::isfinite(y)) return;
    const float clip_x = 2.f * x / static_cast<float>(width);
    const float clip_y = 2.f * y / static_cast<float>(height);
    for (unsigned column = 0; column != 4; ++column)
    {
        matrix[column * 4] += clip_x * matrix[column * 4 + 3];
        matrix[column * 4 + 1] += clip_y * matrix[column * 4 + 3];
    }
}

void alMetalExpandTemporalCullProjection(float matrix[16], std::uint32_t width,
    std::uint32_t height) noexcept
{
    if (!matrix || !width || !height) return;
    const float scale_x = static_cast<float>(static_cast<double>(width) / (static_cast<double>(width) + 2.));
    const float scale_y = static_cast<float>(static_cast<double>(height) / (static_cast<double>(height) + 2.));
    for (unsigned column = 0; column != 4; ++column)
    {
        matrix[column * 4] *= scale_x;
        matrix[column * 4 + 1] *= scale_y;
    }
}

struct ALMetalTemporalWorldHistory::Impl
{
    struct Entry { Key key; Pose pose; };
    struct Frame
    {
        Input input;
        std::uint32_t phase = 0;
        std::vector<Entry> entries;
        std::unordered_map<Key, std::size_t, Hash> lookup;
        std::size_t bytes = sizeof(Input);
    };
    Limits limits;
    std::unique_ptr<Frame> committed, pending;
    std::vector<Pose> previous_opaque, previous_forward;
    std::array<float,16> clip_to_previous = identity();
    std::array<float,2> jitter{};
    Stats counters;
    bool reset = true, staged = false;

    explicit Impl(Limits value) : limits(value) {}
    void clearPending() noexcept
    {
        pending.reset(); previous_opaque.clear(); previous_forward.clear();
        staged = false; reset = true; clip_to_previous = identity(); jitter = {};
    }
};

ALMetalTemporalWorldHistory::ALMetalTemporalWorldHistory() : ALMetalTemporalWorldHistory(Limits{}) {}
ALMetalTemporalWorldHistory::ALMetalTemporalWorldHistory(Limits limits) : mImpl(std::make_unique<Impl>(limits)) {}
ALMetalTemporalWorldHistory::~ALMetalTemporalWorldHistory() = default;

bool ALMetalTemporalWorldHistory::beginFrame(const FrameInput& in, std::string* error)
{
    auto& state = *mImpl;
    if (state.pending) return fail(error, "A Temporal world history transaction is already pending");
    if (!in.world_id || (state.committed && in.world_id == state.committed->input.world_id) ||
        !in.input_width || !in.input_height || in.output_width < in.input_width ||
        in.output_height < in.input_height || !finite(in.camera_mvp) ||
        !std::all_of(in.origin.begin(), in.origin.end(), [](double x) { return std::isfinite(x); }))
        return fail(error, "Temporal history requires a new world identity, valid dimensions and finite camera/origin");
    auto clip = identity();
    if (!previousClip(in.camera_mvp, in.camera_mvp, clip))
        return fail(error, "Temporal camera projection is not invertible");
    try
    {
        auto pending = std::make_unique<Impl::Frame>();
        pending->input = in;
        state.reset = in.force_reset || !state.committed || !compatible(in, state.committed->input);
        pending->phase = state.reset ? 0u : (state.committed->phase + 1u) % 16u;
        if (!state.reset && !previousClip(in.camera_mvp, state.committed->input.camera_mvp, clip))
            return fail(error, "Temporal current-to-previous camera projection could not be formed");
        state.clip_to_previous = state.reset ? identity() : clip;
        state.jitter = {halton(pending->phase + 1u, 2), halton(pending->phase + 1u, 3)};
        state.pending = std::move(pending);
        state.staged = false;
        state.counters.valid_previous_draws = state.counters.rejected_previous_draws = 0;
        return true;
    }
    catch (const std::bad_alloc&)
    { return fail(error, "Could not allocate the bounded Temporal world transaction"); }
}

bool ALMetalTemporalWorldHistory::stage(const ALMetalWorldGBufferScene& opaque,
    const ALMetalWorldOpaquePostDeferredScene* forward, std::string* error)
{
    auto& state = *mImpl;
    if (!state.pending || state.staged)
        return fail(error, "Temporal instance staging requires one unstaged pending world");
    const auto opaque_count = opaque.drawCount();
    const auto forward_count = forward ? forward->batch_count : 0;
    const auto reject = [&](const char* text)
    { invalidate(); return fail(error, text); };
    if (opaque_count > state.limits.draws || forward_count > state.limits.draws - opaque_count ||
        (forward_count && !forward->batches))
        return reject("Temporal instance count exceeds the bounded history or forward storage is missing");
    const std::size_t count = opaque_count + forward_count;
    // Conservative per-row accounting includes both current and previous pose
    // records, map keys/nodes/buckets and deep palette copies. No prior full
    // world beyond the immediately preceding submitted world is retained.
    constexpr std::size_t row_bytes = sizeof(Impl::Entry) + sizeof(Pose) + sizeof(Key) + 128u;
    if (state.limits.bytes_per_frame < sizeof(Input) ||
        count > (state.limits.bytes_per_frame - sizeof(Input)) / row_bytes)
        return reject("Temporal pose descriptors exceed the bounded frame-storage allowance");
    try
    {
        auto& next = *state.pending;
        next.bytes = sizeof(Input) + count * row_bytes;
        next.entries.reserve(count); next.lookup.reserve(count);
        state.previous_opaque.resize(opaque_count); state.previous_forward.resize(forward_count);
        const auto add = [&](const ALMetalWorldDraw& draw, bool is_forward) -> bool
        {
            const auto& source = draw.source();
            if (source.material_slot_count > source.max_material_slots ||
                source.pbr_slot_count > source.max_material_slots || source.texture_count > source.max_diffuse_textures)
                return fail(error, "Temporal material table or diffuse texture count exceeds its layout");
            Impl::Entry entry;
            entry.key = key(draw, is_forward);
            const auto palette_count = draw.skinPaletteFloatCount();
            if (palette_count > (state.limits.bytes_per_frame - next.bytes) / (2u * sizeof(float)))
                return fail(error, "Temporal deep palette snapshots exceed the bounded frame-storage allowance");
            next.bytes += palette_count * 2u * sizeof(float);
            if (!snapshot(draw, entry.pose, error)) return false;
            if (!entry.key.values[1] && !entry.key.values[4]) entry.pose.valid = false;
            const std::size_t index = next.entries.size();
            const auto [found, inserted] = next.lookup.emplace(entry.key, index);
            if (!inserted)
            {
                // Ambiguous same-family copies never inherit a plausible but
                // unrelated pose. Both current rows and the next world reject.
                next.entries[found->second].pose.valid = false;
                entry.pose.valid = false;
            }
            next.entries.push_back(std::move(entry));
            return true;
        };
        for (std::size_t i = 0; i != opaque_count; ++i)
        {
            if (!alMetalWorldDrawStorageAvailable(opaque, i))
                return reject("Temporal opaque draw storage or immutable-source ownership is invalid");
            if (!add(opaque.draw(i), false)) { invalidate(); return false; }
        }
        for (std::size_t i = 0; i != forward_count; ++i)
            if (!add(ALMetalWorldDraw(forward->batches[i]), true)) { invalidate(); return false; }
        for (std::size_t i = 0; i != count; ++i)
        {
            const auto& current = next.entries[i];
            Pose* previous = i < opaque_count ? &state.previous_opaque[i] : &state.previous_forward[i - opaque_count];
            if (!state.reset && current.pose.valid)
            {
                const auto found = state.committed->lookup.find(current.key);
                if (found != state.committed->lookup.end())
                {
                    const auto& pose = state.committed->entries[found->second].pose;
                    if (pose.valid)
                    {
                        *previous = pose;
                        previous->reactive = previous->reactive || current.pose.reactive ||
                            std::memcmp(previous->texture_matrix, current.pose.texture_matrix,
                                        sizeof(previous->texture_matrix)) != 0;
                    }
                }
            }
            if (!previous->valid)
            {
                previous->reactive = true;
                ++state.counters.rejected_previous_draws;
            }
            else ++state.counters.valid_previous_draws;
        }
        state.staged = true;
        return true;
    }
    catch (const std::bad_alloc&)
    { return reject("Could not allocate bounded Temporal pose snapshots"); }
}

bool ALMetalTemporalWorldHistory::commit(std::uint64_t world_id, std::string* error) noexcept
{
    auto& state = *mImpl;
    if (!state.pending || !state.staged || state.pending->input.world_id != world_id)
        return fail(error, "Temporal history commit does not identify a staged world transaction");
    if (state.reset) ++state.counters.resets;
    state.committed = std::move(state.pending);
    ++state.counters.submitted_worlds;
    state.clearPending();
    return true;
}

void ALMetalTemporalWorldHistory::abandon(std::uint64_t world_id) noexcept
{
    auto& state = *mImpl;
    if (state.pending && (!world_id || state.pending->input.world_id == world_id))
    { ++state.counters.abandoned_worlds; state.clearPending(); }
}
void ALMetalTemporalWorldHistory::invalidate() noexcept
{ mImpl->committed.reset(); mImpl->clearPending(); }
bool ALMetalTemporalWorldHistory::pending() const noexcept { return bool(mImpl->pending); }
bool ALMetalTemporalWorldHistory::resetRequired() const noexcept { return mImpl->reset; }
std::uint64_t ALMetalTemporalWorldHistory::pendingWorld() const noexcept
{ return mImpl->pending ? mImpl->pending->input.world_id : 0; }
std::uint64_t ALMetalTemporalWorldHistory::committedWorld() const noexcept
{ return mImpl->committed ? mImpl->committed->input.world_id : 0; }
std::array<float,2> ALMetalTemporalWorldHistory::jitterPixels() const noexcept { return mImpl->jitter; }
const std::array<float,16>& ALMetalTemporalWorldHistory::clipToPrevious() const noexcept
{ return mImpl->clip_to_previous; }
const std::vector<Pose>& ALMetalTemporalWorldHistory::previousOpaque() const noexcept
{ return mImpl->previous_opaque; }
const std::vector<Pose>& ALMetalTemporalWorldHistory::previousForward() const noexcept
{ return mImpl->previous_forward; }
ALMetalTemporalWorldHistory::Stats ALMetalTemporalWorldHistory::stats() const noexcept
{
    auto out = mImpl->counters;
    if (mImpl->committed)
    { out.committed_draws = mImpl->committed->entries.size(); out.committed_bytes = mImpl->committed->bytes; }
    if (mImpl->pending)
    { out.pending_draws = mImpl->pending->entries.size(); out.pending_bytes = mImpl->pending->bytes; }
    return out;
}
