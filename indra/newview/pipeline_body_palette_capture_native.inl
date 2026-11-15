/**
 * @file pipeline_body_palette_capture_native.inl
 * @brief Owner-side publication of the required view-composed body palette.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * The mesh remains the authoritative pose/modelview composition owner.
 */
namespace ALMetalBodyPaletteCaptureNative
{
template<class Mesh>
std::shared_ptr<const ALMetalBodyPaletteRecord> capture(const Mesh& mesh)
{
    // This copy replaces the old adoption snapshot. It does not reuse a
    // palette from another camera or skip the original body composition.
    std::array<float,45*4> values{};
    if(!mesh.buildJointMatrixPalette(values.data(),values.size()))return {};
    return ALMetalBodyPaletteRecord::create(values);
}
}
