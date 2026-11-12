// Standalone research adapter; the viewer does not link this codec.
#include "benchmark.hpp"
#include "grok.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace {
thread_local std::string last_error;
void quiet(const char*, void*) {}
void error(const char* msg, void*) { last_error = msg ? msg : "Grok decode failure"; }
struct Release { void operator()(grk_object* p) const { grk_object_unref(p); } };
[[noreturn]] void fail(const char* stage) {
    throw std::runtime_error(std::string(stage) + (last_error.empty() ? "" : ": " + last_error));
}
}

namespace tdbench {
Decoded decode(const std::vector<uint8_t>& bytes, int reduce) {
    last_error.clear();
    if(bytes.empty() || reduce < 0 || reduce > 32)
        throw std::runtime_error("Invalid Grok input/reduction");
    grk_stream_params stream{};
    stream.buf = const_cast<uint8_t*>(bytes.data());
    stream.buf_len = bytes.size();
    grk_decompress_parameters params{};
    params.core.reduce = static_cast<uint8_t>(reduce);
    params.num_threads = 1; // Independent inline executor per codec.
    params.cpu_only = true;
    std::unique_ptr<grk_object, Release> codec(grk_decompress_init(&stream, &params));
    if(!codec) fail("Grok init");
    grk_header_info header{};
    if(!grk_decompress_read_header(codec.get(), &header)) fail("Grok header");
    if(!grk_decompress(codec.get(), nullptr)) fail("Grok decode");
    grk_image* image = grk_decompress_get_image(codec.get());
    if(!image || !image->comps || image->numcomps == 0 || image->numcomps > 4)
        fail("Grok unsupported component count");
    Decoded out{};
    out.width = image->comps[0].w;
    out.height = image->comps[0].h;
    out.components = image->numcomps;
    const uint64_t count = uint64_t(out.width) * out.height * out.components;
    if(!out.width || !out.height || count > 268435456)
        fail("Grok invalid image size");
    out.pixels.resize(static_cast<size_t>(count));
    for(uint32_t c=0; c<out.components; ++c) {
        const auto& comp = image->comps[c];
        if(!comp.data || comp.w != out.width || comp.h != out.height ||
           comp.stride < out.width || comp.prec != 8 || comp.sgnd ||
           (comp.data_type != GRK_INT_16 && comp.data_type != GRK_INT_32))
            fail("Grok unsupported component layout");
        for(uint32_t y=0; y<out.height; ++y) {
            for(uint32_t x=0; x<out.width; ++x) {
                const auto src = size_t(y) * comp.stride + x;
                const int32_t sample = comp.data_type == GRK_INT_16 ?
                    static_cast<const int16_t*>(comp.data)[src] :
                    static_cast<const int32_t*>(comp.data)[src];
                out.pixels[(size_t(y) * out.width + x) * out.components + c] =
                    static_cast<uint8_t>(sample);
            }
        }
    }
    return out;
}
}

int main(int argc, char** argv) {
    grk_msg_handlers handlers{};
    handlers.info_callback = quiet;
    handlers.debug_callback = quiet;
    handlers.trace_callback = quiet;
    handlers.warn_callback = quiet;
    handlers.error_callback = error;
    grk_set_msg_handlers(handlers);
    grk_initialize(nullptr, 1, nullptr);
    const int status = tdbench::run(argc, argv, "grok", tdbench::decode);
    grk_deinitialize();
    return status;
}
