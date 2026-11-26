//! Bounded, GPU-resident cost measurement of the unchanged Aurora graph.
//! No shader or reconstruction decision is modified. Supported Metal render-stage
//! timestamps enclose the input adapter and every FSR pass.
use super::*;
use objc2_metal::{MTLDevice, MTLResource};
use serde::Serialize;
use std::{
    collections::VecDeque,
    ffi::CStr,
    sync::{
        OnceLock,
        atomic::{AtomicBool, AtomicU32, Ordering},
    },
    time::{Duration, Instant},
};

const D: Dimensions = Dimensions {
    width: 2304,
    height: 1446,
    output_width: 3456,
    output_height: 2168,
};
const PHASES: usize = 18;
const IN_FLIGHT: usize = 3;
unsafe extern "C" {
    fn ae_workload_version() -> *const std::ffi::c_char;
    fn ae_workload_jitter(phase: u32, out: *mut f32);
    fn ae_workload_projection(out: *mut f32);
    fn ae_workload_fill(phase: u32, rgba: *mut u16, depth: *mut f32, motion: *mut u16) -> i32;
    fn ae_workload_fingerprint(bytes: *const std::ffi::c_void, count: u64, initial: u64) -> u64;
}
static QUERIES: OnceLock<wgpu::QuerySet> = OnceLock::new();
static FRAME_QUERY_BASE: AtomicU32 = AtomicU32::new(u32::MAX);
static RCAS_TIMING: AtomicBool = AtomicBool::new(false);
/// Apple GPUs expose render-stage timestamps, not arbitrary encoder writes.
/// Observe adapter begin/end, first FSR begin and the two terminal ends.
/// Intermediate stages keep timestamp_writes=None.
pub(crate) fn pass_timing(label: &str) -> Option<wgpu::RenderPassTimestampWrites<'static>> {
    let base = FRAME_QUERY_BASE.load(Ordering::Relaxed);
    if base == u32::MAX {
        return None;
    }
    let query_set = QUERIES.get()?;
    match label {
        "fsr autogen reactive" => Some(wgpu::RenderPassTimestampWrites {
            query_set,
            beginning_of_pass_write_index: Some(base + 2),
            end_of_pass_write_index: None,
        }),
        "fsr display depth" => Some(wgpu::RenderPassTimestampWrites {
            query_set,
            beginning_of_pass_write_index: None,
            end_of_pass_write_index: Some(base + 4),
        }),
        "fsr accumulate" if !RCAS_TIMING.load(Ordering::Relaxed) => {
            Some(wgpu::RenderPassTimestampWrites {
                query_set,
                beginning_of_pass_write_index: None,
                end_of_pass_write_index: Some(base + 3),
            })
        }
        "fsr rcas" if RCAS_TIMING.load(Ordering::Relaxed) => {
            Some(wgpu::RenderPassTimestampWrites {
                query_set,
                beginning_of_pass_write_index: None,
                end_of_pass_write_index: Some(base + 3),
            })
        }
        _ => None,
    }
}
struct ResidentPhase {
    color: wgpu::Texture,
    depth: wgpu::Texture,
    motion: wgpu::Texture,
}
struct Pending {
    submission: wgpu::SubmissionIndex,
    sample: Option<usize>,
    world: usize,
    phase: usize,
    cpu_encode_ms: f64,
    cpu_submit_ms: f64,
    producer_wait_ms: f64,
}
#[derive(Serialize)]
struct Sample {
    sample: usize,
    world: usize,
    phase: usize,
    adapter_start_tick: u64,
    adapter_end_tick: u64,
    first_fsr_begin_tick: u64,
    color_end_tick: u64,
    display_depth_end_tick: u64,
    display_depth_minus_color_end_ticks: i64,
    last_fsr_end_tick: u64,
    total_gpu_ms: f64,
    adapter_gpu_ms: f64,
    fsr_pass_envelope_gpu_ms: f64,
    adapter_to_first_fsr_gpu_gap_ms: f64,
    cpu_encode_wall_ms: f64,
    cpu_submit_wall_ms: f64,
    input_producer_wait_wall_ms: f64,
    completion_gate_wall_ms: f64,
}
fn memory_snapshot(gpu: &Gpu) -> serde_json::Value {
    let c = gpu.device.get_internal_counters().hal;
    let native = unsafe { gpu.device.as_hal::<wgpu::hal::api::Metal>() }
        .map(|h| h.raw_device().currentAllocatedSize());
    json!({"metal_device_current_allocated_bytes":native,"wgpu_hal_active":{"buffers":c.buffers.read(),"textures":c.textures.read(),"texture_views":c.texture_views.read(),"buffer_memory":c.buffer_memory.read(),"texture_memory":c.texture_memory.read(),"memory_allocations":c.memory_allocations.read(),"command_encoders":c.command_encoders.read(),"query_sets":c.query_sets.read()},"helper_cumulative_texture_creations":renderer::created_texture_count()})
}
fn texture_inventory(name: &str, t: &wgpu::Texture) -> serde_json::Value {
    let physical =
        unsafe { t.as_hal::<wgpu::hal::api::Metal>() }.map(|t| t.raw_handle().allocatedSize());
    json!({"name":name,"width":t.width(),"height":t.height(),"mip_levels":t.mip_level_count(),"format":format!("{:?}",t.format()),"metal_allocated_bytes":physical})
}
fn distribution(values: &[f64]) -> serde_json::Value {
    let mut v = values.to_vec();
    v.sort_by(f64::total_cmp);
    let n = v.len();
    let median = if n % 2 == 0 {
        (v[n / 2 - 1] + v[n / 2]) / 2.
    } else {
        v[n / 2]
    };
    json!({"count":n,"min_ms":v[0],"median_ms":median,"p95_ms":v[((n as f64*0.95).ceil()as usize)-1],"max_ms":v[n-1],"mean_ms":v.iter().sum::<f64>()/n as f64})
}
fn make_pool(gpu: &Gpu) -> Result<(Vec<ResidentPhase>, Vec<String>)> {
    let count = (D.width * D.height) as usize;
    let mut rgba = vec![0u16; count * 4];
    let mut depth = vec![0f32; count];
    let mut motion = vec![0u16; count * 2];
    let usage = wgpu::TextureUsages::COPY_SRC | wgpu::TextureUsages::COPY_DST;
    let mut pool = Vec::with_capacity(PHASES);
    let mut fingerprints = Vec::new();
    for phase in 0..PHASES {
        ensure!(
            unsafe {
                ae_workload_fill(
                    phase as u32,
                    rgba.as_mut_ptr(),
                    depth.as_mut_ptr(),
                    motion.as_mut_ptr(),
                )
            } == 1,
            "Shared input producer failed"
        );
        let mut hash = 14695981039346656037u64;
        for bytes in [
            bytemuck::cast_slice::<u16, u8>(rgba.as_slice()),
            bytemuck::cast_slice::<f32, u8>(depth.as_slice()),
            bytemuck::cast_slice::<u16, u8>(motion.as_slice()),
        ] {
            hash =
                unsafe { ae_workload_fingerprint(bytes.as_ptr().cast(), bytes.len() as u64, hash) };
        }
        fingerprints.push(format!("{hash:016x}"));
        let phase = ResidentPhase {
            color: texture(
                &gpu.device,
                (D.width, D.height),
                wgpu::TextureFormat::Rgba16Float,
                usage,
            ),
            depth: texture(
                &gpu.device,
                (D.width, D.height),
                wgpu::TextureFormat::R32Float,
                usage,
            ),
            motion: texture(
                &gpu.device,
                (D.width, D.height),
                wgpu::TextureFormat::Rg16Float,
                usage,
            ),
        };
        for (target, bytes, bpp) in [
            (&phase.color, bytemuck::cast_slice(rgba.as_slice()), 8),
            (&phase.depth, bytemuck::cast_slice(depth.as_slice()), 4),
            (&phase.motion, bytemuck::cast_slice(motion.as_slice()), 4),
        ] {
            gpu.queue.write_texture(
                target.as_image_copy(),
                bytes,
                wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(D.width * bpp),
                    rows_per_image: Some(D.height),
                },
                target.size(),
            );
        }
        // Bound the producer's one-time staging memory; none of these uploads
        // are in a warm-up/measured command's GPU interval or CPU sample.
        gpu.queue.submit([]);
        gpu.device.poll(wgpu::PollType::wait_indefinitely())?;
        pool.push(phase);
    }
    Ok((pool, fingerprints))
}
fn prepare(gpu: &Gpu, stream: &Stream, phase: &ResidentPhase) -> wgpu::SubmissionIndex {
    let mut encoder = gpu
        .device
        .create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("UNTIMED resident phase copy (stand-in for scene production)"),
        });
    for (src, dst) in [
        (&phase.color, &stream.color),
        (&phase.depth, &stream.raw_depth),
        (&phase.motion, &stream.raw_motion),
    ] {
        encoder.copy_texture_to_texture(src.as_image_copy(), dst.as_image_copy(), src.size());
    }
    gpu.queue.submit([encoder.finish()])
}
fn complete(gpu: &Gpu, pending: Pending, out: &mut Vec<Sample>) -> Result<()> {
    let gate = Instant::now();
    gpu.device.poll(wgpu::PollType::Wait {
        submission_index: Some(pending.submission),
        timeout: Some(Duration::from_secs(60)),
    })?;
    if let Some(sample) = pending.sample {
        out.push(Sample {
            sample,
            world: pending.world,
            phase: pending.phase,
            adapter_start_tick: 0,
            adapter_end_tick: 0,
            first_fsr_begin_tick: 0,
            color_end_tick: 0,
            display_depth_end_tick: 0,
            display_depth_minus_color_end_ticks: 0,
            last_fsr_end_tick: 0,
            total_gpu_ms: 0.,
            adapter_gpu_ms: 0.,
            fsr_pass_envelope_gpu_ms: 0.,
            adapter_to_first_fsr_gpu_gap_ms: 0.,
            cpu_encode_wall_ms: pending.cpu_encode_ms,
            cpu_submit_wall_ms: pending.cpu_submit_ms,
            input_producer_wait_wall_ms: pending.producer_wait_ms,
            completion_gate_wall_ms: gate.elapsed().as_secs_f64() * 1000.,
        });
    }
    Ok(())
}
fn copy_queries(gpu: &Gpu, query: &wgpu::QuerySet, count: u32) -> Result<Vec<u64>> {
    let size = u64::from(count) * 8;
    let resolve = gpu.device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("AFTER measurement query resolve"),
        size,
        usage: wgpu::BufferUsages::QUERY_RESOLVE | wgpu::BufferUsages::COPY_SRC,
        mapped_at_creation: false,
    });
    let readback = gpu.device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("AFTER measurement query readback"),
        size,
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
        mapped_at_creation: false,
    });
    let mut e = gpu.device.create_command_encoder(&Default::default());
    e.resolve_query_set(query, 0..count, &resolve, 0);
    e.copy_buffer_to_buffer(&resolve, 0, &readback, 0, size);
    gpu.queue.submit([e.finish()]);
    let (tx, rx) = mpsc::channel();
    readback.map_async(wgpu::MapMode::Read, .., move |r| {
        let _ = tx.send(r);
    });
    gpu.device.poll(wgpu::PollType::wait_indefinitely())?;
    rx.recv()??;
    let bytes = readback.get_mapped_range(..)?;
    let result = bytes
        .chunks_exact(8)
        .map(|b| u64::from_le_bytes(b.try_into().unwrap()))
        .collect();
    drop(bytes);
    readback.unmap();
    Ok(result)
}
fn encode(
    gpu: &Gpu,
    stream: &mut Stream,
    motion: &wgpu::TextureView,
    depth: &wgpu::TextureView,
    query: Option<(&wgpu::QuerySet, u32)>,
    world: usize,
    sharpness: Option<f32>,
    projection: Mat4,
) -> Result<(wgpu::SubmissionIndex, f64, f64)> {
    let begin = Instant::now();
    let mut encoder = gpu
        .device
        .create_command_encoder(&wgpu::CommandEncoderDescriptor {
            label: Some("TIMED complete Aurora reconstruction including required input adapter"),
        });
    {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("Required R32-to-Depth32 and pixel-to-UV adapter"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: motion,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT),
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: depth,
                depth_ops: Some(wgpu::Operations {
                    load: wgpu::LoadOp::Clear(0.),
                    store: wgpu::StoreOp::Store,
                }),
                stencil_ops: None,
            }),
            timestamp_writes: query.map(|(q, i)| wgpu::RenderPassTimestampWrites {
                query_set: q,
                beginning_of_pass_write_index: Some(i),
                end_of_pass_write_index: Some(i + 1),
            }),
            occlusion_query_set: None,
            multiview_mask: None,
        });
        pass.set_pipeline(&stream.adapter);
        pass.set_bind_group(0, &stream.group, &[]);
        pass.draw(0..3, 0..1);
    }
    FRAME_QUERY_BASE.store(query.map(|(_, i)| i).unwrap_or(u32::MAX), Ordering::Relaxed);
    let mut jitter = [0.; 2];
    unsafe { ae_workload_jitter((world % PHASES) as u32, jitter.as_mut_ptr()) };
    stream.fsr.encode(
        &gpu.queue,
        &mut encoder,
        &fsr::FrameInputs {
            motion_reproj: Mat4::IDENTITY.to_cols_array_2d(),
            proj: projection,
            jitter: Vec2::from_array(jitter),
            reset: world == 0,
            motion_on: true,
            sharpness,
        },
    );
    let command = encoder.finish();
    let encode_ms = begin.elapsed().as_secs_f64() * 1000.;
    let start = Instant::now();
    let submission = gpu.queue.submit([command]);
    let submit_ms = start.elapsed().as_secs_f64() * 1000.;
    Ok((submission, encode_ms, submit_ms))
}
pub(crate) fn run(args: &[String]) -> Result<()> {
    ensure!(
        !args.is_empty(),
        "benchmark OUT [--rcas off|ordinary] [--samples360] [--warmup360] [--validation]"
    );
    let out = PathBuf::from(&args[0]);
    ensure!(!out.exists(), "Benchmark output must be fresh");
    let (mut sharpness, mut arm, mut samples, mut warm, mut validation) =
        (None, "off", 360usize, 360usize, false);
    let mut quiescent = false;
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--rcas" => {
                i += 1;
                arm = args.get(i).context("RCAS value")?;
                sharpness = match arm {
                    "off" => None,
                    "ordinary" => Some(contract::ORDINARY_SHARPNESS),
                    _ => bail!("Only the two existing arms are permitted"),
                };
            }
            "--samples" => {
                i += 1;
                samples = args.get(i).context("sample count")?.parse()?;
            }
            "--warmup" => {
                i += 1;
                warm = args.get(i).context("warm count")?.parse()?;
            }
            "--validation" => validation = true,
            "--quiescent" => quiescent = true,
            _ => bail!("Unknown benchmark option {}", args[i]),
        }
        i += 1;
    }
    ensure!(
        samples > 0 && samples <= 800 && warm > 0,
        "Invalid bounded sample count"
    );
    ensure!(
        validation || (samples >= 300 && warm >= 360),
        "Timed study requires>=300 samples and>=360warm worlds"
    );
    let env: serde_json::Map<_, _> = [
        "MTL_DEBUG_LAYER",
        "MTL_SHADER_VALIDATION",
        "MTL_SHADER_VALIDATION_ENABLE_PIPELINES",
        "WGPU_VALIDATION",
        "WGPU_DEBUG",
    ]
    .into_iter()
    .filter_map(|key| {
        std::env::var(key)
            .ok()
            .map(|v| (key.to_owned(), serde_json::Value::String(v)))
    })
    .collect();
    if !validation {
        ensure!(
            env.values().all(|v| v == "0"),
            "Unset validation/debug environment for timing"
        );
        ensure!(
            !cfg!(debug_assertions),
            "Use cargo build --release for optimized measurements"
        );
    }
    let in_flight_limit = if quiescent { 1 } else { IN_FLIGHT };
    let gpu = Gpu::new_options(validation, true)?;
    let before_inputs = memory_snapshot(&gpu);
    let (pool, fingerprints) = make_pool(&gpu)?;
    let after_inputs = memory_snapshot(&gpu);
    let mut stream = Stream::new(&gpu, D)?;
    let motion = stream.motion.create_view(&Default::default());
    let depth = stream.depth.create_view(&Default::default());
    let query = gpu.device.create_query_set(&wgpu::QuerySetDescriptor {
        label: Some("Five terminal stage boundaries; no intermediate FSR instrumentation"),
        ty: wgpu::QueryType::Timestamp,
        count: (5 * samples) as u32,
    });
    QUERIES
        .set(query)
        .map_err(|_| anyhow::anyhow!("Single benchmark stream per process"))?;
    let query = QUERIES.get().unwrap();
    let mut projection = [0.; 16];
    unsafe { ae_workload_projection(projection.as_mut_ptr()) };
    let projection = Mat4::from_cols_array(&projection);
    contract::projection_contract(projection)?;
    RCAS_TIMING.store(sharpness.is_some(), Ordering::Relaxed);
    let mut pending = VecDeque::new();
    let mut observed = Vec::with_capacity(samples);
    for world in 0..warm {
        let producer_submission = prepare(&gpu, &stream, &pool[world % PHASES]);
        let wait_begin = Instant::now();
        if quiescent {
            gpu.device.poll(wgpu::PollType::Wait {
                submission_index: Some(producer_submission),
                timeout: Some(Duration::from_secs(60)),
            })?;
        }
        let producer_wait_ms = if quiescent {
            wait_begin.elapsed().as_secs_f64() * 1000.
        } else {
            0.
        };
        let (submission, enc, sub) = encode(
            &gpu,
            &mut stream,
            &motion,
            &depth,
            Some((query, 0)),
            world,
            sharpness,
            projection,
        )?;
        pending.push_back(Pending {
            submission,
            sample: None,
            world,
            phase: world % PHASES,
            cpu_encode_ms: enc,
            cpu_submit_ms: sub,
            producer_wait_ms,
        });
        if pending.len() >= in_flight_limit {
            complete(&gpu, pending.pop_front().unwrap(), &mut observed)?;
        }
    }
    while let Some(p) = pending.pop_front() {
        complete(&gpu, p, &mut observed)?;
    }
    let after_warmup = memory_snapshot(&gpu);
    let mut owned: Vec<_> = stream
        .fsr
        .evaluation_owned_textures()
        .into_iter()
        .map(|(name, t)| texture_inventory(name, t))
        .collect();
    owned.push(texture_inventory("adapter_depth", &stream.depth));
    owned.push(texture_inventory("adapter_motion", &stream.motion));
    let buffers = stream.fsr.evaluation_owned_buffer_payload();
    for sample in 0..samples {
        let world = warm + sample;
        let producer_submission = prepare(&gpu, &stream, &pool[world % PHASES]);
        let wait_begin = Instant::now();
        if quiescent {
            gpu.device.poll(wgpu::PollType::Wait {
                submission_index: Some(producer_submission),
                timeout: Some(Duration::from_secs(60)),
            })?;
        }
        let producer_wait_ms = if quiescent {
            wait_begin.elapsed().as_secs_f64() * 1000.
        } else {
            0.
        };
        let (submission, enc, sub) = encode(
            &gpu,
            &mut stream,
            &motion,
            &depth,
            Some((query, (sample * 5) as u32)),
            world,
            sharpness,
            projection,
        )?;
        pending.push_back(Pending {
            submission,
            sample: Some(sample),
            world,
            phase: world % PHASES,
            cpu_encode_ms: enc,
            cpu_submit_ms: sub,
            producer_wait_ms,
        });
        if pending.len() >= in_flight_limit {
            complete(&gpu, pending.pop_front().unwrap(), &mut observed)?;
        }
    }
    while let Some(p) = pending.pop_front() {
        complete(&gpu, p, &mut observed)?;
    }
    let after_measurement = memory_snapshot(&gpu);
    let ticks = copy_queries(&gpu, query, (5 * samples) as u32)?;
    let period = f64::from(gpu.queue.get_timestamp_period());
    observed.sort_by_key(|s| s.sample);
    for s in &mut observed {
        s.adapter_start_tick = ticks[5 * s.sample];
        s.adapter_end_tick = ticks[5 * s.sample + 1];
        s.first_fsr_begin_tick = ticks[5 * s.sample + 2];
        s.color_end_tick = ticks[5 * s.sample + 3];
        s.display_depth_end_tick = ticks[5 * s.sample + 4];
        s.last_fsr_end_tick = s.color_end_tick.max(s.display_depth_end_tick);
        ensure!(
            s.adapter_end_tick > s.adapter_start_tick
                && s.color_end_tick > s.first_fsr_begin_tick
                && s.display_depth_end_tick > 0,
            "Invalid supported stage-boundary timestamps"
        );
        s.adapter_gpu_ms = (s.adapter_end_tick - s.adapter_start_tick) as f64 * period / 1e6;
        s.total_gpu_ms = (s.last_fsr_end_tick - s.adapter_start_tick.min(s.first_fsr_begin_tick))
            as f64
            * period
            / 1e6;
        s.fsr_pass_envelope_gpu_ms =
            (s.last_fsr_end_tick - s.first_fsr_begin_tick) as f64 * period / 1e6;
        s.adapter_to_first_fsr_gpu_gap_ms =
            (s.first_fsr_begin_tick as i128 - s.adapter_end_tick as i128) as f64 * period / 1e6;
        s.display_depth_minus_color_end_ticks =
            (s.display_depth_end_tick as i128 - s.color_end_tick as i128) as i64;
    }
    ensure!(observed.len() == samples, "Incomplete timing samples");
    let summary = json!({"total_gpu":distribution(&observed.iter().map(|s|s.total_gpu_ms).collect::<Vec<_>>()),"adapter_gpu":distribution(&observed.iter().map(|s|s.adapter_gpu_ms).collect::<Vec<_>>()),"fsr_pass_envelope_gpu":distribution(&observed.iter().map(|s|s.fsr_pass_envelope_gpu_ms).collect::<Vec<_>>()),"adapter_to_first_fsr_gap_gpu":distribution(&observed.iter().map(|s|s.adapter_to_first_fsr_gpu_gap_ms).collect::<Vec<_>>()),"cpu_encode_wall":distribution(&observed.iter().map(|s|s.cpu_encode_wall_ms).collect::<Vec<_>>()),"cpu_submit_wall":distribution(&observed.iter().map(|s|s.cpu_submit_wall_ms).collect::<Vec<_>>()),"excluded_input_producer_wait_wall":distribution(&observed.iter().map(|s|s.input_producer_wait_wall_ms).collect::<Vec<_>>()),"excluded_completion_gate_wall":distribution(&observed.iter().map(|s|s.completion_gate_wall_ms).collect::<Vec<_>>())});
    let validation_result = if validation {
        let values = half_values(&read_texture(&gpu, stream.fsr.output().texture(), 8)?);
        ensure!(
            values.iter().all(|x| x.is_finite()),
            "Nonfinite target-sized output"
        );
        json!({"full_output_finite":true,"scalars":values.len(),"min":values.iter().copied().fold(f32::INFINITY,f32::min),"max":values.iter().copied().fold(f32::NEG_INFINITY,f32::max)})
    } else {
        serde_json::Value::Null
    };
    fs::create_dir_all(&out)?;
    let data = json!({"schema_version":1,"scope":"Reconstruction cost workload, not new quality qualification, Home FPS or future native-MSL estimate","device":gpu.info(),"dimensions":D,"rcas":arm,"sharpness":sharpness,"validation_enabled":validation,"debug_assertions":cfg!(debug_assertions),"environment":env,"warm_worlds":warm,"measured_samples":samples,"in_flight_limit":in_flight_limit,"quiescent":quiescent,"execution_mode":if quiescent{"single pending world; resident input copy completed before timed encoding"}else{"three pending worlds with fixed resources; stage envelopes may contain previous-world waits"},"workload_version":unsafe{CStr::from_ptr(ae_workload_version())}.to_str()?,"phase_fingerprints_fnv64":fingerprints,"jitter_phases":PHASES,"projection_column_major":projection.to_cols_array(),"reprojection":"identity; static perspective geometry, zero explicit motion, actual jittered phase inputs","opaque_is_final_same_view":true,"timestamp_period_ns":period,"timing_boundaries":{"total":"Supported M1 Max stage timestamps: earliest adapter/autogen beginning to latest color-terminal/display-depth end, including input conversion, reconstructed-depth clear, every FSR graph pass and optional RCAS","adapter":"Supported M1 Max render-pass timestamp begin/end, duration only; ticks are not calibrated to native command-buffer seconds","excluded":"CPU input generation/uploads, phase-copy preparation CB, waits, query resolution/readback, output validation/readback and file IO;208-byte per-world uniform staging belongs to normal host encoding/queue write and its GPU copy is outside observed reconstruction CB","fsr_pass_envelope":"First autogen stage beginning to latest color/display-depth terminal end; does not include preceding reconstructed-depth clear. Complete total above includes that clear.","cpu":"Wall encode and queue-submit intervals measured separately; deliberate waits occur outside both"},"memory":{"before_inputs":before_inputs,"after_inputs":after_inputs,"after_warmup":after_warmup,"after_measurement":after_measurement,"owned_textures_actual_allocations":owned,"fsr_buffer_payload_bytes":{"params":buffers.0,"reconstructed_depth":buffers.1,"new_locks_source_descriptor":buffers.2},"resident_input_pool_payload_bytes":u64::from(D.width)*u64::from(D.height)*16*PHASES as u64,"fixed_input_payload_bytes":u64::from(D.width)*u64::from(D.height)*16,"query_set_count":5*samples,"post_measurement_query_buffers_payload_bytes":5*samples*8*2,"limitation":"Device/counter values are live footprints and net growth, not a cumulative count of invisible driver allocations. FSR-owned allocation calls are only in construction; command encoders and208-byte uniform staging remain ordinary backend activity."},"validation_result":validation_result,"summary":summary,"samples":observed});
    fs::write(
        out.join("benchmark.json"),
        serde_json::to_vec_pretty(&data)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&summary)?);
    Ok(())
}
