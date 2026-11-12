#include "benchmark.hpp"
#include "decoder.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace tdbench {
Decoded decode(const std::vector<uint8_t>& bytes, int reduce)
{
    if (bytes.empty() || reduce < 0 || reduce > 32)
        throw std::runtime_error("invalid input or reduction");

    // OpenHTJ2K copies the compressed input into its padded buffer. The common
    // benchmark input is immutable and does not promise the readable 16-byte
    // tail required by init_borrow(). Each request uses a fresh decoder.
    open_htj2k::openhtj2k_decoder decoder(bytes.data(), bytes.size(),
                                         static_cast<uint8_t>(reduce), 1);
    decoder.parse();
    const uint16_t components = decoder.get_num_component();
    if (components == 0 || components > 4)
        throw std::runtime_error("expected 1-4 SL texture components");
    for (uint16_t c = 0; c < components; ++c)
        if (decoder.get_component_depth(c) != 8 || decoder.get_component_signedness(c))
            throw std::runtime_error("expected unsigned 8-bit SL texture components");

    struct Planes {
        std::vector<int32_t*> data;
        ~Planes() { for (auto* p : data) delete[] p; }
    } planes;
    std::vector<uint32_t> widths, heights;
    std::vector<uint8_t> depths;
    std::vector<bool> signs;
    decoder.invoke_line_based(planes.data, widths, heights, depths, signs);
    if (widths.size() != components || heights.size() != components || planes.data.size() != components)
        throw std::runtime_error("unexpected OpenHTJ2K output component count");
    const uint32_t width = widths[0], height = heights[0];
    if (!width || !height)
        throw std::runtime_error("empty decoded texture");
    for (uint16_t c = 0; c < components; ++c)
        if (widths[c] != width || heights[c] != height)
            throw std::runtime_error("subsampled components are outside SL benchmark normalization");
    Decoded result;
    result.width = width;
    result.height = height;
    result.components = components;
    const size_t samples = static_cast<size_t>(width) * height;
    result.pixels.resize(samples * components);
    for (size_t p = 0; p < samples; ++p)
        for (uint16_t c = 0; c < components; ++c)
            result.pixels[p * components + c] = static_cast<uint8_t>(planes.data[c][p]);
    return result;
}
} // namespace tdbench

int main(int argc, char** argv)
{
    return tdbench::run(argc, argv, "openhtj2k", tdbench::decode);
}
