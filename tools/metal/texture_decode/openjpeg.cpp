#include "benchmark.hpp"
#include <openjpeg.h>
#include <cstring>
#include <memory>

namespace {
struct Input { const uint8_t* data; size_t size, offset=0; };
OPJ_SIZE_T read_bytes(void* out, OPJ_SIZE_T size, void* user) {
    auto& in=*static_cast<Input*>(user);
    if(in.offset==in.size) return OPJ_SIZE_T(-1);
    size=std::min(size,in.size-in.offset); std::memcpy(out,in.data+in.offset,size); in.offset+=size; return size;
}
OPJ_OFF_T skip_bytes(OPJ_OFF_T amount, void* user) {
    auto& in=*static_cast<Input*>(user);
    auto next=static_cast<OPJ_OFF_T>(in.offset)+amount;
    if(next<0 || uint64_t(next)>in.size) {
        in.offset=size_t(std::clamp<OPJ_OFF_T>(next,0,static_cast<OPJ_OFF_T>(in.size)));
        return -1;
    }
    in.offset=size_t(next); return amount;
}
OPJ_BOOL seek_bytes(OPJ_OFF_T pos, void* user) {
    auto& in=*static_cast<Input*>(user);
    if(pos<0 || uint64_t(pos)>in.size)return OPJ_FALSE;
    in.offset=size_t(pos);return OPJ_TRUE;
}
void message(const char* text,void* user){*static_cast<std::string*>(user)=text;}
tdbench::Decoded decode(const std::vector<uint8_t>& bytes,int reduce) {
    std::string error;
    std::unique_ptr<opj_codec_t,decltype(&opj_destroy_codec)> codec(opj_create_decompress(OPJ_CODEC_J2K),opj_destroy_codec);
    std::unique_ptr<opj_stream_t,decltype(&opj_stream_destroy)> stream(opj_stream_create(bytes.size(),OPJ_TRUE),opj_stream_destroy);
    if(!codec||!stream)throw std::runtime_error("OpenJPEG allocation failed");
    opj_dparameters_t p;opj_set_default_decoder_parameters(&p);
    // OpenJPEG uses 0 for inline execution; 1 creates an extra worker for each
    // fresh codec. The viewer uses the default inline path when its env is unset.
    if(!opj_setup_decoder(codec.get(),&p)||!opj_codec_set_threads(codec.get(),0))throw std::runtime_error("OpenJPEG setup failed");
    opj_set_error_handler(codec.get(),message,&error);
    opj_set_warning_handler(codec.get(),nullptr,nullptr);opj_set_info_handler(codec.get(),nullptr,nullptr);
    opj_decoder_set_strict_mode(codec.get(),OPJ_FALSE);
    Input in{bytes.data(),bytes.size()};opj_stream_set_user_data(stream.get(),&in,nullptr);
    opj_stream_set_user_data_length(stream.get(),bytes.size());
    opj_stream_set_read_function(stream.get(),read_bytes);opj_stream_set_skip_function(stream.get(),skip_bytes);opj_stream_set_seek_function(stream.get(),seek_bytes);
    opj_image_t* raw=nullptr;
    if(!opj_read_header(stream.get(),codec.get(),&raw))throw std::runtime_error(error.empty()?"OpenJPEG header failed":error);
    std::unique_ptr<opj_image_t,decltype(&opj_image_destroy)> image(raw,opj_image_destroy);
    unsigned effective=unsigned(reduce);
    auto* info=opj_get_cstr_info(codec.get());
    if(info){if(info->m_default_tile_info.tccp_info){auto n=info->m_default_tile_info.tccp_info[0].numresolutions;if(n)effective=std::min(effective,n-1);}opj_destroy_cstr_info(&info);}
    if(!opj_set_decoded_resolution_factor(codec.get(),effective)||!opj_decode(codec.get(),stream.get(),image.get()))
        throw std::runtime_error(error.empty()?"OpenJPEG decode failed":error);
    opj_end_decompress(codec.get(),stream.get());
    if(!image->numcomps||image->numcomps>4)throw std::runtime_error("unsupported components");
    tdbench::Decoded out;out.width=image->comps[0].w;out.height=image->comps[0].h;out.components=image->numcomps;
    out.pixels.resize(size_t(out.width)*out.height*out.components);
    for(unsigned c=0;c<out.components;++c){auto& comp=image->comps[c];
        if(comp.prec!=8||comp.sgnd||comp.w!=out.width||comp.h!=out.height||!comp.data)throw std::runtime_error("unsupported component layout");
        for(size_t p=0;p<size_t(out.width)*out.height;++p)out.pixels[p*out.components+c]=uint8_t(comp.data[p]);
    }
    return out;
}
}
int main(int argc,char** argv){return tdbench::run(argc,argv,"openjpeg",decode);}
