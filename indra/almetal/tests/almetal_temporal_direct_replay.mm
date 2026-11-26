/**
 * @file almetal_temporal_direct_replay.mm
 * @brief Standalone native MetalFX replay of a complete generated palm sequence.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_temporal_direct_fixture.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
using Direct=almetal_test::DirectTemporalFixture;
using Color=std::array<float,4>;
static_assert(sizeof(Color)==16);
namespace fs=std::filesystem;

std::vector<std::string> columns(std::string line)
{
    if(!line.empty() && line.back()=='\r')line.pop_back();
    if(line.find('"')!=std::string::npos)throw std::runtime_error("Expected fixed, unquoted fixture CSV schema");
    std::vector<std::string> result;std::stringstream stream(line);std::string value;
    while(std::getline(stream,value,','))result.push_back(value);
    return result;
}
std::uint64_t integer(const std::string& text)
{
    std::uint64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())throw std::runtime_error("Invalid unsigned CSV integer: "+text);
    return value;
}
float number(const std::string& text)
{
    std::size_t end=0;const float value=std::stof(text,&end);
    if(end!=text.size() || !std::isfinite(value))throw std::runtime_error("Invalid finite CSV number: "+text);
    return value;
}
template<class T>std::vector<T> read(const fs::path& path,std::size_t count)
{
    if(count>std::numeric_limits<std::size_t>::max()/sizeof(T))throw std::runtime_error("Input size overflow");
    const auto bytes=count*sizeof(T);std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=static_cast<std::streamoff>(bytes))throw std::runtime_error("Missing/wrong-sized input: "+path.string());
    file.seekg(0);std::vector<T> result(count);file.read(reinterpret_cast<char*>(result.data()),bytes);
    if(!file)throw std::runtime_error("Could not read input: "+path.string());return result;
}
std::string indexName(std::uint64_t frame)
{std::ostringstream name;name<<std::setw(3)<<std::setfill('0')<<frame;return name.str();}
std::uint32_t dimension(const std::string& text)
{
    const auto value=integer(text);
    if(!value || value>16384)throw std::runtime_error("Fixture dimension outside Apple Silicon 2D texture bounds");
    return static_cast<std::uint32_t>(value);
}
}

int main(int argc,char** argv)
{
    if(argc!=4){std::cerr<<"usage: almetal_temporal_direct_replay SEQUENCE_CSV FRAME_PREFIX OUTPUT_DIR\n";return 64;}
    @autoreleasepool
    {
        try
        {
            static_assert(std::endian::native==std::endian::little);
            const auto sequence=fs::absolute(argv[1]);auto prefix=fs::path(argv[2]);
            if(prefix.is_relative())prefix=sequence.parent_path()/prefix;
            const auto output=fs::absolute(argv[3]);
            if(fs::exists(output) && !fs::is_empty(output))throw std::runtime_error("Output directory must be fresh");
            fs::create_directories(output);std::ifstream csv(sequence);std::string line;
            if(!csv || !std::getline(csv,line))throw std::runtime_error("Missing sequence CSV header");
            const auto header=columns(line);std::unordered_map<std::string,std::size_t> keys;
            for(std::size_t i=0;i<header.size();++i)if(!keys.emplace(header[i],i).second)throw std::runtime_error("Duplicate CSV column");
            for(const auto* key:{"frame","world_id","phase","jitter_x","jitter_y","camera_shift","reset","history_resets","input_width","input_height","output_width","output_height","depth_reversed","pre_exposure"})
                if(!keys.contains(key))throw std::runtime_error(std::string("Missing CSV field: ")+key);
            auto device=MTLCreateSystemDefaultDevice();if(!device)throw std::runtime_error("No native Metal device");
            std::ofstream identity(output/"device.txt");identity<<device.name.UTF8String<<'\n'<<device.registryID<<'\n';identity.flush();
            if(!identity)throw std::runtime_error("Could not write native device identity");
            std::ofstream rows(output/"frames.csv");rows<<std::setprecision(17);
            rows<<"frame,world_id,phase,jitter_x,jitter_y,camera_shift,reset,depth_reversed,pre_exposure,rgb_components,rgb_differences,rgb_max_error,alpha_differences,alpha_nonfinite,alpha_max_error\n";
            std::unique_ptr<Direct> direct;Direct::Config config;std::string error;
            std::uint64_t frames=0,rgb_differences=0,alpha_differences=0,alpha_nonfinite=0;double rgb_max=0,alpha_max=0;
            while(std::getline(csv,line))
            {
                if(line.empty())throw std::runtime_error("Empty sequence row");
                const auto values=columns(line);if(values.size()!=header.size())throw std::runtime_error("Incomplete sequence row");
                const auto field=[&](const char* key)->const std::string&{return values[keys.at(key)];};
                const auto frame=integer(field("frame")),world=integer(field("world_id")),phase=integer(field("phase"));
                const auto reset=integer(field("reset")),reverse=integer(field("depth_reversed"));
                if(frame!=frames || world!=frame+1 || phase!=frame%18 || reset!=(frame==0) || reverse>1 || integer(field("history_resets"))!=1)
                    throw std::runtime_error("Noncontiguous world/phase or invalid reset/depth metadata");
                Direct::Config dimensions{dimension(field("input_width")),dimension(field("input_height")),dimension(field("output_width")),dimension(field("output_height")),true};
                if(!direct){config=dimensions;direct=Direct::create(device,config,error);if(!direct)throw std::runtime_error(error);}
                if(dimensions.input_width!=config.input_width || dimensions.input_height!=config.input_height || dimensions.output_width!=config.output_width || dimensions.output_height!=config.output_height)
                    throw std::runtime_error("Sequence changes descriptors without an independent history stream");
                const auto input_count=std::size_t(config.input_width)*config.input_height,output_count=std::size_t(config.output_width)*config.output_height;
                const auto base=prefix.string()+indexName(frame);
                const auto color=read<Color>(base+"-input.rgba32f",input_count);
                std::vector<Direct::HalfPixel> half(input_count);
                for(std::size_t p=0;p<input_count;++p)for(unsigned c=0;c<4;++c)
                {
                    half[p][c]=color[p][c];
                    if(!std::isfinite(color[p][c]) || float(half[p][c])!=color[p][c])throw std::runtime_error("Source RGBA32F does not preserve exact finite RGBA16F texels");
                }
                const auto depth=read<float>(base+"-proxy-depth.r32f",input_count);
                const auto motion=read<std::array<__fp16,2>>(base+"-motion.rg16f",input_count);
                const auto reactive=read<std::uint8_t>(base+"-reactive.r8",input_count);
                const auto expected=read<Color>(base+"-native-output.rgba32f",output_count);
                Direct::PackedInputs inputs{std::as_bytes(std::span(half)),std::as_bytes(std::span(depth)),std::as_bytes(std::span(motion)),std::as_bytes(std::span(reactive))};
                Direct::Frame metadata{world,number(field("jitter_x")),number(field("jitter_y")),number(field("pre_exposure")),bool(reverse),bool(reset)};
                Direct::Result result;if(!direct->render(inputs,metadata,result,error))throw std::runtime_error(error);
                if(result.world_id!=world || result.rgba16.size()!=output_count)throw std::runtime_error("Direct output does not match this world descriptor");
                std::vector<Color> actual(output_count);std::uint64_t frgb=0,falpha=0,fnan=0;double fmax=0,famax=0;
                for(std::size_t p=0;p<output_count;++p)for(unsigned c=0;c<4;++c)
                {
                    actual[p][c]=result.rgba16[p][c];const double delta=std::abs(actual[p][c]-expected[p][c]);
                    if(c<3){if(!std::isfinite(actual[p][c]) || !std::isfinite(expected[p][c]))throw std::runtime_error("Nonfinite reconstructed RGB");frgb+=delta!=0;fmax=std::max(fmax,delta);}
                    else if(!std::isfinite(actual[p][c]) || !std::isfinite(expected[p][c]))++fnan;
                    else{falpha+=delta!=0;famax=std::max(famax,delta);}
                }
                std::ofstream image(output/("frame-"+indexName(frame)+"-direct-output.rgba32f"),std::ios::binary);
                image.write(reinterpret_cast<const char*>(actual.data()),actual.size()*sizeof(Color));if(!image)throw std::runtime_error("Could not write direct output");
                rows<<frame<<','<<world<<','<<phase<<','<<metadata.jitter_x<<','<<metadata.jitter_y<<','<<number(field("camera_shift"))<<','<<reset<<','<<reverse<<','<<metadata.pre_exposure<<','<<output_count*3<<','<<frgb<<','<<fmax<<','<<falpha<<','<<fnan<<','<<famax<<'\n';
                rgb_differences+=frgb;alpha_differences+=falpha;alpha_nonfinite+=fnan;rgb_max=std::max(rgb_max,fmax);alpha_max=std::max(alpha_max,famax);++frames;
            }
            rows.flush();
            if(!direct || (frames!=144 && frames!=54) || !csv.eof() || !rows)throw std::runtime_error("Incomplete generated main/reference sequence (expected144 or54worlds)");
            std::ofstream summary(output/"summary.json");summary<<std::setprecision(17)
                <<"{\n  \"complete\": true,\n  \"frames\": "<<frames<<",\n  \"reset_frames\": "<<direct->resetFrames()<<",\n  \"input_width\": "<<config.input_width<<",\n  \"input_height\": "<<config.input_height<<",\n  \"output_width\": "<<config.output_width<<",\n  \"output_height\": "<<config.output_height<<",\n  \"rgb_components\": "<<frames*config.output_width*config.output_height*3<<",\n  \"rgb_differences\": "<<rgb_differences<<",\n  \"rgb_max_error\": "<<rgb_max<<",\n  \"alpha_differences_observational\": "<<alpha_differences<<",\n  \"alpha_nonfinite_observational\": "<<alpha_nonfinite<<",\n  \"alpha_max_error_observational\": "<<alpha_max<<"\n}\n";
            summary.flush();if(!summary)throw std::runtime_error("Could not write replay summary");
            std::cout<<"Direct native sequence worlds="<<frames<<", RGB differences="<<rgb_differences<<", maximum error="<<rgb_max<<'\n';
            return rgb_differences?2:0;
        }
        catch(const std::exception& error){std::cerr<<"Direct replay failed: "<<error.what()<<'\n';return 1;}
    }
}
