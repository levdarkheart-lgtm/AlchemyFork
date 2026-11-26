/** Evaluation-only native classic MetalFX Temporal reconstruction cost.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * No production viewer source or scheduling behavior is linked or changed.
 */
#include "../workload/input_workload.h"
#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Ns = std::chrono::nanoseconds;
static double ns(Clock::time_point a, Clock::time_point b) {
  return double(std::chrono::duration_cast<Ns>(b - a).count());
}
static void require(bool value, const std::string &text) {
  if (!value)
    throw std::runtime_error(text);
}
static std::string errorText(NSError *e) {
  return e ? e.localizedDescription.UTF8String : "unknown Metal error";
}
static NSString *s(const std::string &value) {
  return [NSString stringWithUTF8String:value.c_str()];
}
static std::string sha256(NSData *data) {
  unsigned char bytes[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(data.bytes, (CC_LONG)data.length, bytes);
  char out[65];
  for (unsigned i = 0; i < 32; ++i)
    snprintf(out + 2 * i, 3, "%02x", bytes[i]);
  return out;
}
struct Config {
  std::string output, library;
  unsigned warmup = 360, samples = 360, inflight = 3;
  bool validate = false, quiescent = false;
};
static Config parse(int argc, char **argv) {
  Config c;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    if (k == "--quiescent") {
      c.quiescent = true;
      continue;
    }
    if (k == "--validate") {
      c.validate = true;
      continue;
    }
    require(i + 1 < argc, "Missing value for " + k);
    const std::string v = argv[++i];
    if (k == "--output")
      c.output = v;
    else if (k == "--library")
      c.library = v;
    else if (k == "--warmup")
      c.warmup = std::stoul(v);
    else if (k == "--samples")
      c.samples = std::stoul(v);
    else if (k == "--max-inflight")
      c.inflight = std::stoul(v);
    else
      throw std::runtime_error("Unknown option " + k);
  }
  require(!c.output.empty() && !c.library.empty(),
          "--output and --library are required");
  if (c.quiescent)
    c.inflight = 1;
  require(c.inflight > 0 && c.inflight <= 3,
          "Use1..3 outstanding worlds, not an unbounded queue");
  require(c.samples > 0 && c.warmup > 0,
          "Warm-up and samples must be positive");
  require(c.validate || (c.samples >= 300 && c.warmup >= 360),
          "Measurement requires>=360warmup and>=300samples");
  require(![[NSFileManager defaultManager] fileExistsAtPath:s(c.output)],
          "Output already exists; preserve previous runs");
  const auto on = [](const char *key) {
    const char *value = getenv(key);
    return value && std::string(value) != "0";
  };
  if (!c.validate)
    for (const char *key : {"MTL_DEBUG_LAYER", "MTL_SHADER_VALIDATION",
                            "MTL_CAPTURE_ENABLED", "DYLD_INSERT_LIBRARIES"})
      require(!on(key),
              std::string("Unset validation/capture for measurement: ") + key);
  return c;
}
struct Textures {
  id<MTLTexture> color = nil, depth = nil, motion = nil;
  std::array<float, 2> jitter{};
};
struct Sample {
  unsigned index = 0, world = 0, phase = 0;
  double encode_ns = 0, submit_ns = 0, producer_encode_ns = 0,
         producer_submit_ns = 0, gate_ns = 0, gpu_start = 0, gpu_end = 0,
         producer_gpu_start = 0, producer_gpu_end = 0,
         excluded_producer_wait_ns = 0;
  uint64_t allocated_bytes = 0;
};
struct Pending {
  id<MTLCommandBuffer> command = nil, producer = nil;
  int sample = -1;
};
struct Calibration {
  MTLTimestamp cpu = 0, gpu = 0;
};
struct Runner {
  Config config;
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  id<MTLFXTemporalScaler> scaler = nil;
  id<MTLRenderPipelineState> resolve_pipeline = nil;
  id<MTLSamplerState> sampler = nil;
  std::array<Textures, AE_PHASES> phases;
  Textures fixed;
  id<MTLTexture> raw = nil, resolved = nil, reactive = nil, exposure = nil;
  uint64_t app_textures = 0, app_buffers = 0;
  NSMutableArray *texture_ledger = [NSMutableArray array];
  NSMutableArray *fingerprints = [NSMutableArray array];
  uint64_t device_base = 0, after_scaler = 0, after_preparation = 0,
           warm_bytes = 0, end_bytes = 0, observed_min = UINT64_MAX,
           observed_max = 0;
  uint64_t explicit_texture_bytes = 0, staging_peak = 0, phase_pool_bytes = 0,
           fixed_input_bytes = 0;
  std::string library_hash;
  Calibration before{}, after{};
  std::vector<Sample> samples;
  std::deque<Pending> pending;
  double warm_gate_ns = 0, final_drain_ns = 0;
  NSProcessInfoThermalState thermal_before = NSProcessInfoThermalStateNominal,
                            thermal_after = NSProcessInfoThermalStateNominal;
  explicit Runner(Config c) : config(std::move(c)) {
    for (id<MTLDevice> candidate in MTLCopyAllDevices())
      if ([candidate.name containsString:@"M1 Max"]) {
        device = candidate;
        break;
      }
    require(device && device.hasUnifiedMemory,
            "Required native Apple M1 Max with unified memory not found");
    require([MTLFXTemporalScalerDescriptor supportsDevice:device],
            "Classic MetalFX Temporal unavailable");
    queue = [device newCommandQueue];
    require(queue, "Cannot create command queue");
    device_base = device.currentAllocatedSize;
    auto desc = [MTLFXTemporalScalerDescriptor new];
    desc.inputWidth = AE_INPUT_WIDTH;
    desc.inputHeight = AE_INPUT_HEIGHT;
    desc.outputWidth = AE_OUTPUT_WIDTH;
    desc.outputHeight = AE_OUTPUT_HEIGHT;
    desc.colorTextureFormat = MTLPixelFormatRGBA16Float;
    desc.depthTextureFormat = MTLPixelFormatR32Float;
    desc.motionTextureFormat = MTLPixelFormatRG16Float;
    desc.outputTextureFormat = MTLPixelFormatRGBA16Float;
    desc.autoExposureEnabled = NO;
    desc.requiresSynchronousInitialization = YES;
    desc.inputContentPropertiesEnabled = NO;
    desc.reactiveMaskTextureEnabled = YES;
    desc.reactiveMaskTextureFormat = MTLPixelFormatR8Unorm;
    scaler = [desc newTemporalScalerWithDevice:device];
    require(scaler, "MetalFX descriptor creation failed");
    after_scaler = device.currentAllocatedSize;
    NSError *error = nil;
    NSData *library_data = [NSData dataWithContentsOfFile:s(config.library)
                                                  options:0
                                                    error:&error];
    require(library_data, errorText(error));
    library_hash = sha256(library_data);
    require(
        library_hash ==
            "5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a",
        "Resolve library differs from preserved production5d identity");
    auto library =
        [device newLibraryWithURL:[NSURL fileURLWithPath:s(config.library)]
                            error:&error];
    require(library, errorText(error));
    auto pd = [MTLRenderPipelineDescriptor new];
    pd.label = @"Exact Alchemy Temporal glow-alpha resolve";
    pd.vertexFunction =
        [library newFunctionWithName:@"almetal_resample_vertex"];
    pd.fragmentFunction =
        [library newFunctionWithName:@"almetal_world_temporal_color_fragment"];
    require(pd.vertexFunction && pd.fragmentFunction,
            "Preserved resolve entry points absent");
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
    resolve_pipeline = [device newRenderPipelineStateWithDescriptor:pd
                                                              error:&error];
    require(resolve_pipeline, errorText(error));
    auto sd = [MTLSamplerDescriptor new];
    sd.minFilter = MTLSamplerMinMagFilterLinear;
    sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler = [device newSamplerStateWithDescriptor:sd];
    require(sampler, "Sampler creation failed");
    auto inputs = [&](NSString *prefix) {
      Textures t;
      t.color =
          texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, MTLPixelFormatRGBA16Float,
                  scaler.colorTextureUsage | MTLTextureUsageShaderRead,
                  [prefix stringByAppendingString:@" color"]);
      t.depth = texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, MTLPixelFormatR32Float,
                        scaler.depthTextureUsage | MTLTextureUsageShaderRead,
                        [prefix stringByAppendingString:@" reverse depth"]);
      t.motion =
          texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, MTLPixelFormatRG16Float,
                  scaler.motionTextureUsage | MTLTextureUsageShaderRead,
                  [prefix stringByAppendingString:@" pixel motion"]);
      return t;
    };
    for (unsigned i = 0; i < AE_PHASES; ++i) {
      phases[i] = inputs([NSString stringWithFormat:@"Resident phase%u", i]);
      ae_workload_jitter(i, phases[i].jitter.data());
      phase_pool_bytes += phases[i].color.allocatedSize +
                          phases[i].depth.allocatedSize +
                          phases[i].motion.allocatedSize;
    }
    fixed = inputs(@"Fixed reconstruction inputs");
    fixed_input_bytes = fixed.color.allocatedSize + fixed.depth.allocatedSize +
                        fixed.motion.allocatedSize;
    raw = texture(AE_OUTPUT_WIDTH, AE_OUTPUT_HEIGHT, MTLPixelFormatRGBA16Float,
                  scaler.outputTextureUsage | MTLTextureUsageShaderRead,
                  @"Raw native MetalFX output");
    resolved =
        texture(AE_OUTPUT_WIDTH, AE_OUTPUT_HEIGHT, MTLPixelFormatRGBA16Float,
                MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead,
                @"Glow-alpha resolved HDR output");
    reactive = texture(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, MTLPixelFormatR8Unorm,
                       scaler.reactiveTextureUsage | MTLTextureUsageShaderRead,
                       @"Qualified opaque MASK rejection0");
    exposure = texture(1, 1, MTLPixelFormatR16Float, MTLTextureUsageShaderRead,
                       @"Unit exposure", MTLStorageModeShared);
    const __fp16 one = 1.f;
    [exposure replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                mipmapLevel:0
                  withBytes:&one
                bytesPerRow:2];
    prepare();
    after_preparation = device.currentAllocatedSize;
    scaler.colorTexture = fixed.color;
    scaler.depthTexture = fixed.depth;
    scaler.motionTexture = fixed.motion;
    scaler.outputTexture = raw;
    scaler.exposureTexture = exposure;
    scaler.reactiveMaskTexture = reactive;
    scaler.inputContentWidth = AE_INPUT_WIDTH;
    scaler.inputContentHeight = AE_INPUT_HEIGHT;
    scaler.preExposure = 1.f;
    scaler.motionVectorScaleX = 1.f;
    scaler.motionVectorScaleY = 1.f;
    scaler.depthReversed = YES;
    samples.resize(config.samples);
  }
  id<MTLTexture> texture(unsigned width, unsigned height, MTLPixelFormat format,
                         MTLTextureUsage usage, NSString *label,
                         MTLStorageMode storage = MTLStorageModePrivate) {
    auto d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                width:width
                                                               height:height
                                                            mipmapped:NO];
    d.storageMode = storage;
    d.usage = usage;
    d.hazardTrackingMode = MTLHazardTrackingModeTracked;
    auto t = [device newTextureWithDescriptor:d];
    require(t, "Texture allocation failed");
    t.label = label;
    ++app_textures;
    explicit_texture_bytes += t.allocatedSize;
    [texture_ledger addObject:@{
      @"label" : label,
      @"width" : @(width),
      @"height" : @(height),
      @"format" : @(format),
      @"allocated_size_bytes" : @(t.allocatedSize),
      @"usage" : @(usage),
      @"storage" : @(storage)
    }];
    return t;
  }
  void finish(id<MTLCommandBuffer> command) {
    [command waitUntilCompleted];
    require(command.status == MTLCommandBufferStatusCompleted,
            "GPU command failed: " + errorText(command.error));
  }
  void prepare() {
    const size_t count = size_t(AE_INPUT_WIDTH) * AE_INPUT_HEIGHT,
                 color_bytes = count * 8, depth_bytes = count * 4,
                 motion_bytes = count * 4,
                 total = color_bytes + depth_bytes + motion_bytes;
    @autoreleasepool {
      id<MTLBuffer> staging =
          [device newBufferWithLength:total
                              options:MTLResourceStorageModeShared];
      require(staging, "Cannot allocate preparation staging");
      ++app_buffers;
      staging_peak = staging.allocatedSize;
      auto *rgba = static_cast<uint16_t *>(staging.contents);
      auto *depth = reinterpret_cast<float *>(
          static_cast<char *>(staging.contents) + color_bytes);
      auto *motion = reinterpret_cast<uint16_t *>(
          static_cast<char *>(staging.contents) + color_bytes + depth_bytes);
      for (unsigned phase = 0; phase < AE_PHASES; ++phase) {
        require(ae_workload_fill(phase, rgba, depth, motion),
                "Shared workload generator rejected phase");
        uint64_t fingerprint = ae_workload_fingerprint(
            rgba, color_bytes, UINT64_C(14695981039346656037));
        fingerprint = ae_workload_fingerprint(depth, depth_bytes, fingerprint);
        fingerprint =
            ae_workload_fingerprint(motion, motion_bytes, fingerprint);
        [fingerprints
            addObject:[NSString
                          stringWithFormat:@"%016llx",
                                           (unsigned long long)fingerprint]];
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLBlitCommandEncoder> copy = [command blitCommandEncoder];
        const auto copy_one = [&](id<MTLTexture> t, size_t offset,
                                  size_t stride) {
          [copy copyFromBuffer:staging
                     sourceOffset:offset
                sourceBytesPerRow:stride
              sourceBytesPerImage:stride * AE_INPUT_HEIGHT
                       sourceSize:MTLSizeMake(AE_INPUT_WIDTH, AE_INPUT_HEIGHT,
                                              1)
                        toTexture:t
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake(0, 0, 0)];
        };
        copy_one(phases[phase].color, 0, AE_INPUT_WIDTH * 8);
        copy_one(phases[phase].depth, color_bytes, AE_INPUT_WIDTH * 4);
        copy_one(phases[phase].motion, color_bytes + depth_bytes,
                 AE_INPUT_WIDTH * 4);
        [copy endEncoding];
        [command commit];
        finish(command);
      }
      memset(staging.contents, 0, count);
      id<MTLCommandBuffer> command = [queue commandBuffer];
      auto copy = [command blitCommandEncoder];
      [copy copyFromBuffer:staging
                 sourceOffset:0
            sourceBytesPerRow:AE_INPUT_WIDTH
          sourceBytesPerImage:count
                   sourceSize:MTLSizeMake(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, 1)
                    toTexture:reactive
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
      [copy endEncoding];
      [command commit];
      finish(command);
    }
  }
  void retire() {
    require(!pending.empty(), "No pending world");
    const auto a = Clock::now();
    auto p = pending.front();
    finish(p.command);
    require(p.producer.status == MTLCommandBufferStatusCompleted,
            "Resident producer failed: " + errorText(p.producer.error));
    const auto b = Clock::now();
    if (p.sample >= 0) {
      auto &v = samples[p.sample];
      v.gate_ns += ns(a, b);
      v.gpu_start = p.command.GPUStartTime;
      v.gpu_end = p.command.GPUEndTime;
      v.producer_gpu_start = p.producer.GPUStartTime;
      v.producer_gpu_end = p.producer.GPUEndTime;
      require(v.producer_gpu_start > 0. &&
                  v.producer_gpu_end > v.producer_gpu_start,
              "Producer GPU timestamps unavailable");
      require(v.gpu_start > 0. && v.gpu_end > v.gpu_start,
              "Metal GPU timestamps unavailable");
    } else
      warm_gate_ns += ns(a, b);
    pending.pop_front();
  }
  void world(unsigned index, int sample_index) {
    @autoreleasepool {
      while (pending.size() >= config.inflight)
        retire();
      const unsigned phase = index % AE_PHASES;
      auto &input = phases[phase];
      Sample *result = sample_index >= 0 ? &samples[sample_index] : nullptr;
      // Identical untimed dummy producer used by both algorithms: copy a
      // resident phase into fixed bound inputs before the measured command.
      const auto pa = Clock::now();
      auto producer = [queue commandBuffer];
      auto copy = [producer blitCommandEncoder];
      const auto cp = [&](id<MTLTexture> from, id<MTLTexture> to) {
        [copy copyFromTexture:from
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(AE_INPUT_WIDTH, AE_INPUT_HEIGHT, 1)
                    toTexture:to
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
      };
      cp(input.color, fixed.color);
      cp(input.depth, fixed.depth);
      cp(input.motion, fixed.motion);
      [copy endEncoding];
      const auto pb = Clock::now();
      [producer commit];
      const auto pc = Clock::now();
      // Isolated component-cost correction: settle the dummy producer before
      // the reconstruction clocks. This host wait is separately observed and
      // does not enter GPU or CPU encode/submission time.
      double producer_wait_ns = 0.;
      if (config.quiescent) {
        const auto wait_begin = Clock::now();
        finish(producer);
        producer_wait_ns = ns(wait_begin, Clock::now());
      }
      const auto a = Clock::now();
      auto command = [queue commandBuffer];
      command.label =
          @"Native MetalFX Temporal plus unchanged glow-alpha resolve";
      scaler.jitterOffsetX = input.jitter[0];
      scaler.jitterOffsetY = input.jitter[1];
      scaler.reset = index == 0;
      [scaler encodeToCommandBuffer:command];
      auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = resolved;
      pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      auto encoder = [command renderCommandEncoderWithDescriptor:pass];
      require(encoder, "Resolve encoder unavailable");
      [encoder setViewport:MTLViewport{0., 0., double(AE_OUTPUT_WIDTH),
                                       double(AE_OUTPUT_HEIGHT), 0., 1.}];
      [encoder setScissorRect:MTLScissorRect{0, 0, AE_OUTPUT_WIDTH,
                                             AE_OUTPUT_HEIGHT}];
      [encoder setCullMode:MTLCullModeNone];
      [encoder setRenderPipelineState:resolve_pipeline];
      [encoder setFragmentTexture:raw atIndex:0];
      [encoder setFragmentTexture:fixed.color atIndex:1];
      [encoder setFragmentSamplerState:sampler atIndex:0];
      const float offsets[4]{input.jitter[0] / float(AE_INPUT_WIDTH),
                             input.jitter[1] / float(AE_INPUT_HEIGHT), 0, 0};
      [encoder setFragmentBytes:offsets length:sizeof(offsets) atIndex:0];
      [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                  vertexStart:0
                  vertexCount:3];
      [encoder endEncoding];
      const auto b = Clock::now();
      [command commit];
      const auto c = Clock::now();
      if (result) {
        result->index = sample_index;
        result->world = index + 1;
        result->phase = phase;
        result->encode_ns = ns(a, b);
        result->submit_ns = ns(b, c);
        result->producer_encode_ns = ns(pa, pb);
        result->producer_submit_ns = ns(pb, pc);
        result->excluded_producer_wait_ns = producer_wait_ns;
        result->allocated_bytes = device.currentAllocatedSize;
        observed_min = std::min(observed_min, result->allocated_bytes);
        observed_max = std::max(observed_max, result->allocated_bytes);
      }
      pending.push_back({command, producer, sample_index});
    }
  }
  NSDictionary *validate_output() {
    const size_t row = AE_OUTPUT_WIDTH * 8, bytes = row * AE_OUTPUT_HEIGHT;
    auto buffer = [device newBufferWithLength:bytes
                                      options:MTLResourceStorageModeShared];
    require(buffer, "Validation readback allocation failed");
    auto command = [queue commandBuffer];
    auto copy = [command blitCommandEncoder];
    [copy copyFromTexture:resolved
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(AE_OUTPUT_WIDTH, AE_OUTPUT_HEIGHT,
                                             1)
                        toBuffer:buffer
               destinationOffset:0
          destinationBytesPerRow:row
        destinationBytesPerImage:bytes];
    [copy endEncoding];
    [command commit];
    finish(command);
    const auto *half = static_cast<const __fp16 *>(buffer.contents);
    double sum = 0.;
    float minimum = INFINITY, maximum = -INFINITY;
    size_t finite = 0;
    for (size_t i = 0; i < bytes / 2; ++i) {
      float v = half[i];
      finite += std::isfinite(v);
      minimum = std::min(minimum, v);
      maximum = std::max(maximum, v);
      sum += v;
    }
    require(finite == bytes / 2 && maximum > 0.f,
            "Validation produced nonfinite or empty output");
    return @{
      @"finite_components" : @(finite),
      @"minimum" : @(minimum),
      @"maximum" : @(maximum),
      @"sum" : @(sum),
      @"readback_outside_gpu_and_cpu_timing" : @YES
    };
  }
  static NSDictionary *distribution(const std::vector<double> &values) {
    require(!values.empty(), "Empty distribution");
    auto v = values;
    std::sort(v.begin(), v.end());
    double sum = 0.;
    for (double x : v)
      sum += x;
    auto q = [&](double p) {
      return v[std::min(v.size() - 1, size_t(std::ceil(p * v.size()) - 1))];
    };
    return @{
      @"count" : @(v.size()),
      @"mean" : @(sum / v.size()),
      @"min" : @(v.front()),
      @"p50" : @(q(.5)),
      @"p95" : @(q(.95)),
      @"p99" : @(q(.99)),
      @"max" : @(v.back())
    };
  }
  void run() {
    for (unsigned i = 0; i < config.warmup; ++i)
      world(i, -1);
    while (!pending.empty())
      retire();
    warm_bytes = device.currentAllocatedSize;
    thermal_before = [NSProcessInfo processInfo].thermalState;
    const auto texture_count = app_textures, buffer_count = app_buffers;
    [device sampleTimestamps:&before.cpu gpuTimestamp:&before.gpu];
    for (unsigned i = 0; i < config.samples; ++i)
      world(config.warmup + i, (int)i);
    const auto da = Clock::now();
    while (!pending.empty())
      retire();
    const auto db = Clock::now();
    final_drain_ns = ns(da, db);
    [device sampleTimestamps:&after.cpu gpuTimestamp:&after.gpu];
    end_bytes = device.currentAllocatedSize;
    thermal_after = [NSProcessInfo processInfo].thermalState;
    require(texture_count == app_textures && buffer_count == app_buffers,
            "Steady state allocated app-owned GPU resources");
    std::vector<double> gpu, encode, submit, producer, gate, input_ready, gap,
        producer_wait;
    uint64_t overlaps = 0;
    NSMutableArray *rows = [NSMutableArray arrayWithCapacity:samples.size()];
    for (const auto &v : samples) {
      const double elapsed = (v.gpu_end - v.gpu_start) * 1000.;
      gpu.push_back(elapsed);
      input_ready.push_back((v.gpu_end - v.producer_gpu_end) * 1000.);
      gap.push_back((v.gpu_start - v.producer_gpu_end) * 1000.);
      overlaps += v.producer_gpu_end > v.gpu_start;
      encode.push_back(v.encode_ns / 1e6);
      submit.push_back(v.submit_ns / 1e6);
      producer.push_back((v.producer_encode_ns + v.producer_submit_ns) / 1e6);
      gate.push_back(v.gate_ns / 1e6);
      producer_wait.push_back(v.excluded_producer_wait_ns / 1e6);
      [rows addObject:@{
        @"sample" : @(v.index),
        @"world_id" : @(v.world),
        @"phase" : @(v.phase),
        @"gpu_start_seconds" : @(v.gpu_start),
        @"gpu_end_seconds" : @(v.gpu_end),
        @"gpu_total_ms" : @(elapsed),
        @"producer_gpu_start_seconds" : @(v.producer_gpu_start),
        @"producer_gpu_end_seconds" : @(v.producer_gpu_end),
        @"input_ready_to_completion_ms" :
            @((v.gpu_end - v.producer_gpu_end) * 1000.),
        @"pre_reconstruction_gap_ms" :
            @((v.gpu_start - v.producer_gpu_end) * 1000.),
        @"gpu_adapter_ms" : @0.,
        @"cpu_encode_ns" : @(v.encode_ns),
        @"cpu_submit_ns" : @(v.submit_ns),
        @"untimed_producer_encode_ns" : @(v.producer_encode_ns),
        @"untimed_producer_submit_ns" : @(v.producer_submit_ns),
        @"host_completion_gate_ns" : @(v.gate_ns),
        @"excluded_producer_wait_ns" : @(v.excluded_producer_wait_ns),
        @"device_allocated_bytes_after_submit" : @(v.allocated_bytes)
      }];
    }
    NSDictionary *validation = config.validate
                                   ? validate_output()
                                   : @{@"not_run_in_measurement" : @YES};
    NSMutableDictionary *environment = [NSMutableDictionary dictionary];
    for (const char *key : {"MTL_DEBUG_LAYER", "MTL_SHADER_VALIDATION",
                            "MTL_CAPTURE_ENABLED", "DYLD_INSERT_LIBRARIES"}) {
      const char *value = getenv(key);
      environment[s(key)] = value ? s(value) : [NSNull null];
    }
    NSDictionary *result = @{
      @"schema" : @1,
      @"implementation" : @"Classic MTLFXTemporalScaler plus exact Alchemy "
                          @"RGB/glow-alpha resolve",
      @"cost_workload" : s(ae_workload_version()),
      @"validation_run" : @(config.validate),
      @"environment" : environment,
      @"device" : @{
        @"name" : device.name,
        @"registry_id" : @(device.registryID),
        @"unified_memory" : @(device.hasUnifiedMemory),
        @"os" : [NSProcessInfo processInfo].operatingSystemVersionString,
        @"compiler" : [NSString stringWithUTF8String:__clang_version__]
      },
      @"dimensions" : @{
        @"input_width" : @(AE_INPUT_WIDTH),
        @"input_height" : @(AE_INPUT_HEIGHT),
        @"output_width" : @(AE_OUTPUT_WIDTH),
        @"output_height" : @(AE_OUTPUT_HEIGHT),
        @"phases" : @(AE_PHASES)
      },
      @"thermal_state_before" : @(thermal_before),
      @"thermal_state_after" : @(thermal_after),
      @"warmup_worlds" : @(config.warmup),
      @"measured_worlds" : @(config.samples),
      @"maximum_pending_worlds" : @(config.inflight),
      @"quiescent_component_mode" : @(config.quiescent),
      @"phase_input_fingerprints_fnv64" : fingerprints,
      @"resolve_library_sha256" : s(library_hash),
      @"timing_scope" :
          @"GPUStartTime..GPUEndTime of one command containing native MetalFX "
          @"plus unchanged glow-alpha resolve; includes internal "
          @"execution/dependencies, not exclusive shader self-time. Untimed "
          @"resident producer copy is separate preceding command. Native "
          @"accepts R32 depth/RG16 pixel motion directly: zero reconstruction "
          @"adapter pass. Core/resolve not separately partitioned. Input-ready "
          @"span additionally measures producer GPU-end through main GPU-end "
          @"and includes submission/transfer/gaps; it is not exclusive GPU "
          @"execution time.",
      @"calibration" : @{
        @"cpu_before" : @(before.cpu),
        @"gpu_before" : @(before.gpu),
        @"cpu_after" : @(after.cpu),
        @"gpu_after" : @(after.gpu),
        @"api" : @"MTLDevice sampleTimestamps; raw device/CPU timestamp units; "
                 @"durations use command GPU seconds"
      },
      @"gpu_total_ms" : distribution(gpu),
      @"input_ready_to_completion_ms" : distribution(input_ready),
      @"pre_reconstruction_gap_ms" : distribution(gap),
      @"producer_overlaps_main_samples" : @(overlaps),
      @"cpu_encode_ms" : distribution(encode),
      @"cpu_submit_ms" : distribution(submit),
      @"untimed_cpu_producer_ms" : distribution(producer),
      @"host_completion_gate_ms" : distribution(gate),
      @"excluded_producer_wait_ms" : distribution(producer_wait),
      @"final_drain_wait_ns" : @(final_drain_ns),
      @"memory" : @{
        @"explicit_persistent_texture_bytes" : @(explicit_texture_bytes),
        @"common_resident_phase_pool_bytes" : @(phase_pool_bytes),
        @"fixed_source_input_bytes" : @(fixed_input_bytes),
        @"algorithm_outputs_bytes" :
            @(raw.allocatedSize + resolved.allocatedSize),
        @"explicit_owned_except_common_phase_pool_bytes" :
            @(explicit_texture_bytes - phase_pool_bytes),
        @"observed_warm_residual_device_bytes" :
            @((int64_t)warm_bytes - (int64_t)device_base -
              (int64_t)explicit_texture_bytes),
        @"preparation_staging_peak_bytes" : @(staging_peak),
        @"preparation_staging_retained_during_benchmark" : @NO,
        @"explicit_gpu_texture_creations" : @(app_textures),
        @"explicit_gpu_buffer_creations_preparation" : @(app_buffers),
        @"steady_gpu_resource_creations" : @0,
        @"device_base_bytes" : @(device_base),
        @"after_native_scaler_creation_bytes" : @(after_scaler),
        @"after_preparation_bytes" : @(after_preparation),
        @"after_warmup_bytes" : @(warm_bytes),
        @"steady_observed_min_bytes" : @(observed_min),
        @"steady_observed_max_bytes" : @(observed_max),
        @"after_drain_bytes" : @(end_bytes),
        @"opaque_internal_limit" :
            @"MetalFX does not expose per-resource history/temporary "
            @"allocation inventory. currentAllocatedSize snapshots include "
            @"opaque history, pipeline caches and driver storage; net/observed "
            @"plateau is not proof of no hidden transient allocations.",
        @"textures" : texture_ledger
      },
      @"output_validation" : validation,
      @"samples" : rows
    };
    NSError *error = nil;
    NSData *data = [NSJSONSerialization
        dataWithJSONObject:result
                   options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
                     error:&error];
    require(data, errorText(error));
    NSString *folder = [s(config.output) stringByDeletingLastPathComponent];
    require([[NSFileManager defaultManager] createDirectoryAtPath:folder
                                      withIntermediateDirectories:YES
                                                       attributes:nil
                                                            error:&error],
            errorText(error));
    require([data writeToFile:s(config.output)
                      options:NSDataWritingAtomic
                        error:&error],
            errorText(error));
  }
};
} // namespace
int main(int argc, char **argv) {
  @autoreleasepool {
    @try {
      try {
        Runner runner(parse(argc, argv));
        runner.run();
        return 0;
      } catch (const std::exception &e) {
        fprintf(stderr, "metalfx benchmark: %s\n", e.what());
        return 1;
      }
    } @catch (NSException *e) {
      fprintf(stderr, "metalfx benchmark exception: %s\n", e.reason.UTF8String);
      return 2;
    }
  }
}
