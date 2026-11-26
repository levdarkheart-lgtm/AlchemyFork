#[cfg(not(all(target_os = "macos", target_arch = "aarch64")))]
compile_error!("This evaluation executes only native Metal on Apple Silicon");
mod pipeline_cache;
mod renderer;
include!(concat!(env!("OUT_DIR"), "/aurora_module.rs"));

use alchemy_aurora_eval::{self as contract, Dimensions, Frame, Inputs, Sequence};
use anyhow::{Context, Result, bail, ensure};
use glam::{Mat4, Vec2, Vec3};
use half::f16;
use serde_json::json;
use std::{
    fs,
    path::{Path, PathBuf},
    sync::mpsc,
};

struct Gpu {
    _instance: wgpu::Instance,
    device: wgpu::Device,
    queue: wgpu::Queue,
    info: wgpu::AdapterInfo,
}
impl Gpu {
    fn new() -> Result<Self> {
        let mut descriptor = wgpu::InstanceDescriptor::new_without_display_handle();
        descriptor.backends = wgpu::Backends::METAL;
        descriptor.flags = wgpu::InstanceFlags::VALIDATION | wgpu::InstanceFlags::DEBUG;
        let instance = wgpu::Instance::new(descriptor);
        let adapters = pollster::block_on(instance.enumerate_adapters(wgpu::Backends::METAL));
        let names: Vec<_> = adapters.iter().map(|a| a.get_info().name).collect();
        let adapter = adapters
            .into_iter()
            .find(|a| {
                a.get_info().backend == wgpu::Backend::Metal && a.get_info().name.contains("M1 Max")
            })
            .with_context(|| format!("Required Metal Apple M1 Max adapter not found: {names:?}"))?;
        let info = adapter.get_info();
        let features =
            adapter.features() & wgpu::Features::TEXTURE_ADAPTER_SPECIFIC_FORMAT_FEATURES;
        let (device, queue) =
            pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
                label: Some("Isolated Aurora evaluation"),
                required_features: features,
                required_limits: adapter.limits(),
                ..Default::default()
            }))?;
        Ok(Self {
            _instance: instance,
            device,
            queue,
            info,
        })
    }
    fn info(&self) -> serde_json::Value {
        json!({"name":self.info.name,"backend":format!("{:?}",self.info.backend),"driver":self.info.driver,"driver_info":self.info.driver_info,"compiler":env!("AURORA_EVAL_RUSTC"),"upstream":"ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f","wgpu":"6c13013b3f9c8a7e45154d45d120ada0cbe55a23"})
    }
}
fn texture(
    device: &wgpu::Device,
    dimensions: (u32, u32),
    format: wgpu::TextureFormat,
    usage: wgpu::TextureUsages,
) -> wgpu::Texture {
    device.create_texture(&wgpu::TextureDescriptor {
        label: Some("Harness owned input"),
        size: wgpu::Extent3d {
            width: dimensions.0,
            height: dimensions.1,
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage,
        view_formats: &[],
    })
}
const ADAPTER_SHADER: &str = r#"
@group(0) @binding(0) var raw_depth:texture_2d<f32>;
@group(0) @binding(1) var raw_motion:texture_2d<f32>;
@vertex fn vertex(@builtin(vertex_index) i:u32)->@builtin(position)vec4<f32>{
 let p=array<vec2<f32>,3>(vec2<f32>(-1.0,-1.0),vec2<f32>(3.0,-1.0),vec2<f32>(-1.0,3.0));return vec4<f32>(p[i],0.0,1.0);
}
struct Adapted { @location(0) motion:vec4<f32>, @builtin(frag_depth) depth:f32 };
@fragment fn fragment(@builtin(position) pixel:vec4<f32>)->Adapted {
 let p=vec2<i32>(pixel.xy);let extent=vec2<f32>(textureDimensions(raw_motion));var o:Adapted;
 o.motion=vec4<f32>(0.0,0.0,-textureLoad(raw_motion,p,0).xy/extent);
 o.depth=textureLoad(raw_depth,p,0).x;return o;
}"#;
struct Stream {
    dimensions: Dimensions,
    color: wgpu::Texture,
    raw_depth: wgpu::Texture,
    raw_motion: wgpu::Texture,
    depth: wgpu::Texture,
    motion: wgpu::Texture,
    adapter: wgpu::RenderPipeline,
    group: wgpu::BindGroup,
    fsr: fsr::Fsr,
}
impl Stream {
    fn new(gpu: &Gpu, d: Dimensions) -> Result<Self> {
        let error = gpu.device.push_error_scope(wgpu::ErrorFilter::Validation);
        let bind_copy = wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST;
        let target = wgpu::TextureUsages::TEXTURE_BINDING
            | wgpu::TextureUsages::RENDER_ATTACHMENT
            | wgpu::TextureUsages::COPY_SRC;
        let color = texture(
            &gpu.device,
            (d.width, d.height),
            wgpu::TextureFormat::Rgba16Float,
            bind_copy,
        );
        let raw_depth = texture(
            &gpu.device,
            (d.width, d.height),
            wgpu::TextureFormat::R32Float,
            bind_copy,
        );
        let raw_motion = texture(
            &gpu.device,
            (d.width, d.height),
            wgpu::TextureFormat::Rg16Float,
            bind_copy,
        );
        let depth = texture(
            &gpu.device,
            (d.width, d.height),
            wgpu::TextureFormat::Depth32Float,
            target,
        );
        let motion = texture(
            &gpu.device,
            (d.width, d.height),
            wgpu::TextureFormat::Rgba16Float,
            target,
        );
        let layout = gpu
            .device
            .create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                label: Some("Source adapter"),
                entries: &[0, 1].map(|binding| wgpu::BindGroupLayoutEntry {
                    binding,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: false },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                }),
            });
        let pipeline_layout = gpu
            .device
            .create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: None,
                bind_group_layouts: &[Some(&layout)],
                immediate_size: 0,
            });
        let module = gpu
            .device
            .create_shader_module(wgpu::ShaderModuleDescriptor {
                label: Some("Exact depth and motion convention adapter"),
                source: wgpu::ShaderSource::Wgsl(ADAPTER_SHADER.into()),
            });
        let adapter = gpu
            .device
            .create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some("R32 depth to Depth32 and pixel motion to Aurora UV"),
                layout: Some(&pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &module,
                    entry_point: Some("vertex"),
                    compilation_options: Default::default(),
                    buffers: &[],
                },
                fragment: Some(wgpu::FragmentState {
                    module: &module,
                    entry_point: Some("fragment"),
                    compilation_options: Default::default(),
                    targets: &[Some(wgpu::ColorTargetState {
                        format: wgpu::TextureFormat::Rgba16Float,
                        blend: None,
                        write_mask: wgpu::ColorWrites::ALL,
                    })],
                }),
                primitive: Default::default(),
                depth_stencil: Some(wgpu::DepthStencilState {
                    format: wgpu::TextureFormat::Depth32Float,
                    depth_write_enabled: Some(true),
                    depth_compare: Some(wgpu::CompareFunction::Always),
                    stencil: Default::default(),
                    bias: Default::default(),
                }),
                multisample: Default::default(),
                multiview_mask: None,
                cache: None,
            });
        let group = gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("Unchanged raw input bytes"),
            layout: &layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(
                        &raw_depth.create_view(&Default::default()),
                    ),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::TextureView(
                        &raw_motion.create_view(&Default::default()),
                    ),
                },
            ],
        });
        let color_view = color.create_view(&Default::default());
        let depth_view = depth.create_view(&Default::default());
        let motion_view = motion.create_view(&Default::default());
        // MASK-only scene: the opaque source is literally the final color view.
        let fsr = fsr::Fsr::new(
            &gpu.device,
            (d.width, d.height),
            (d.output_width, d.output_height),
            wgpu::TextureFormat::Depth32Float,
            &fsr::Inputs {
                color: &color_view,
                depth: &depth_view,
                motion: &motion_view,
                opaque: &color_view,
            },
        );
        gpu.device.poll(wgpu::PollType::wait_indefinitely())?;
        if let Some(error) = pollster::block_on(error.pop()) {
            bail!("Metal pipeline validation: {error}");
        }
        Ok(Self {
            dimensions: d,
            color,
            raw_depth,
            raw_motion,
            depth,
            motion,
            adapter,
            group,
            fsr,
        })
    }
    fn encode(
        &mut self,
        gpu: &Gpu,
        input: &Inputs,
        frame: &Frame,
        sharpness: Option<f32>,
    ) -> Result<()> {
        let error = gpu.device.push_error_scope(wgpu::ErrorFilter::Validation);
        let d = self.dimensions;
        for (target, bytes, bpp) in [
            (&self.color, bytemuck::cast_slice(input.color.as_slice()), 8),
            (
                &self.raw_depth,
                bytemuck::cast_slice(input.depth.as_slice()),
                4,
            ),
            (
                &self.raw_motion,
                bytemuck::cast_slice(input.motion_pixels.as_slice()),
                4,
            ),
        ] {
            gpu.queue.write_texture(
                target.as_image_copy(),
                bytes,
                wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(d.width * bpp),
                    rows_per_image: Some(d.height),
                },
                target.size(),
            );
        }
        let mut encoder = gpu
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("Runtime adapters and unchanged Aurora reconstruction"),
            });
        {
            let motion = self.motion.create_view(&Default::default());
            let depth = self.depth.create_view(&Default::default());
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Exact input convention conversion"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &motion,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::TRANSPARENT),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &depth,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(0.),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            pass.set_pipeline(&self.adapter);
            pass.set_bind_group(0, &self.group, &[]);
            pass.draw(0..3, 0..1);
        }
        self.fsr.encode(
            &gpu.queue,
            &mut encoder,
            &fsr::FrameInputs {
                motion_reproj: frame.reprojection,
                proj: Mat4::from_cols_array_2d(&frame.projection),
                jitter: Vec2::from_array(frame.jitter),
                reset: frame.reset,
                motion_on: true,
                sharpness,
            },
        );
        gpu.queue.submit([encoder.finish()]);
        gpu.device.poll(wgpu::PollType::wait_indefinitely())?;
        if let Some(error) = pollster::block_on(error.pop()) {
            bail!("Metal frame validation: {error}");
        }
        Ok(())
    }
}
fn read_texture(gpu: &Gpu, texture: &wgpu::Texture, bpp: u32) -> Result<Vec<u8>> {
    let row = (texture.width() * bpp).div_ceil(256) * 256;
    let buffer = gpu.device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Evaluation readback outside reconstruction"),
        size: u64::from(row) * u64::from(texture.height()),
        usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
        mapped_at_creation: false,
    });
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let mut source = texture.as_image_copy();
    if texture.format().is_depth_stencil_format() {
        source.aspect = wgpu::TextureAspect::DepthOnly;
    }
    encoder.copy_texture_to_buffer(
        source,
        wgpu::TexelCopyBufferInfo {
            buffer: &buffer,
            layout: wgpu::TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(row),
                rows_per_image: Some(texture.height()),
            },
        },
        texture.size(),
    );
    gpu.queue.submit([encoder.finish()]);
    let (tx, rx) = mpsc::channel();
    buffer.map_async(wgpu::MapMode::Read, .., move |result| {
        let _ = tx.send(result);
    });
    gpu.device.poll(wgpu::PollType::wait_indefinitely())?;
    rx.recv()??;
    let mapped = buffer.get_mapped_range(..)?;
    let mut packed = Vec::with_capacity((texture.width() * texture.height() * bpp) as usize);
    for y in 0..texture.height() as usize {
        packed.extend_from_slice(
            &mapped[y * row as usize..y * row as usize + (texture.width() * bpp) as usize],
        );
    }
    drop(mapped);
    buffer.unmap();
    Ok(packed)
}
fn half_values(bytes: &[u8]) -> Vec<f32> {
    bytes
        .chunks_exact(2)
        .map(|x| f16::from_bits(u16::from_le_bytes(x.try_into().unwrap())).to_f32())
        .collect()
}
fn save_output(path: &Path, values: &[f32]) -> Result<()> {
    let mut bytes = Vec::with_capacity(values.len() * 4);
    for value in values {
        bytes.extend_from_slice(&value.to_le_bytes());
    }
    fs::write(path, bytes)?;
    Ok(())
}
fn adapter_checks(gpu: &Gpu, output: &Path) -> Result<()> {
    let d = Dimensions {
        width: 16,
        height: 16,
        output_width: 24,
        output_height: 24,
    };
    let p = Mat4::from_cols(
        glam::Vec4::new(0.5, 0., 0., 0.),
        glam::Vec4::Y,
        glam::Vec4::new(0., 0., 0., -1.),
        glam::Vec4::new(0., 0., 0.1, 0.),
    );
    let r = p * Mat4::from_translation(Vec3::new(-0.1, 0.2, 0.)) * p.inverse();
    let mut observations = Vec::new();
    for (name, reprojection, motion) in [
        ("stationary_identity", Mat4::IDENTITY, [0., 0.]),
        ("zero_motion_camera_fallback", r, [0., 0.]),
        ("explicit_motion_sign_units", r, [-4., 2.]),
    ] {
        let mut stream = Stream::new(gpu, d)?;
        let input = Inputs {
            color: vec![
                [
                    f16::from_f32(0.2),
                    f16::from_f32(0.4),
                    f16::from_f32(0.1),
                    f16::ONE
                ];
                256
            ],
            depth: (0..256)
                .map(|i| {
                    if i % 16 < 6 {
                        0.05
                    } else if i % 16 < 11 {
                        0.025
                    } else {
                        0.
                    }
                })
                .collect(),
            motion_pixels: vec![[f16::from_f32(motion[0]), f16::from_f32(motion[1])]; 256],
        };
        let frame = Frame {
            index: 0,
            world_id: 1,
            phase: 0,
            jitter: [0.25, -0.3],
            reset: true,
            projection: p.to_cols_array_2d(),
            reprojection: reprojection.to_cols_array_2d(),
        };
        stream.encode(gpu, &input, &frame, None)?;
        let packed = half_values(&read_texture(gpu, &stream.motion, 8)?);
        let depth = read_texture(gpu, &stream.depth, 4)?;
        ensure!(
            depth == bytemuck::cast_slice::<f32, u8>(input.depth.as_slice()),
            "Depth import changed actual R32 values"
        );
        for i in 0..256 {
            let expected = contract::pack_motion(motion, 16, 16);
            for c in 0..4 {
                ensure!(
                    packed[4 * i + c] == expected[c].to_f32(),
                    "GPU motion adapter does not match exact half packing"
                );
            }
        }
        let actual = half_values(&read_texture(
            gpu,
            stream.fsr.evaluation_dilated_motion().texture(),
            4,
        )?);
        let reactive = read_texture(gpu, stream.fsr.evaluation_generated_reactive().texture(), 1)?;
        ensure!(
            reactive.iter().all(|x| *x == 0),
            "Opaque==final generated nonzero reactive mask"
        );
        let mut samples = Vec::new();
        for (x, y) in [(2usize, 4usize), (8, 4), (14, 4)] {
            let i = y * 16 + x;
            let uv = Vec2::new((x as f32 + 0.5) / 16., (y as f32 + 0.5) / 16.);
            let expected = contract::input_motion(
                contract::pack_motion(motion, 16, 16),
                reprojection,
                uv,
                input.depth[i],
                Vec2::from_array(frame.jitter) / 16.,
            );
            for c in 0..2 {
                let h = f16::from_f32(expected[c].abs());
                let ulp = f16::from_bits(h.to_bits() + 1).to_f32() - h.to_f32();
                let tolerance = 2. * ulp + 2e-7;
                ensure!(
                    (actual[2 * i + c] - expected[c]).abs() <= tolerance,
                    "{name}: GPU prepare motion {} vs CPU {}",
                    actual[2 * i + c],
                    expected[c]
                );
            }
            samples.push(json!({"pixel":[x,y],"depth":input.depth[i],"expected_uv":expected.to_array(),"actual_uv":[actual[2*i],actual[2*i+1]]}));
        }
        observations.push(json!({"case":name,"depth_copy_exact":true,"packed_motion_exact":true,"generated_reactive_max":0,"samples":samples}));
    }
    fs::write(
        output.join("adapter-checks.json"),
        serde_json::to_vec_pretty(
            &json!({"device":gpu.info(),"cases":observations,"tolerance":"2 half ULP plus2e-7; fixed before execution"}),
        )?,
    )?;
    Ok(())
}
fn main() -> Result<()> {
    contract::reject_overrides()?;
    let args: Vec<_> = std::env::args().collect();
    ensure!(
        args.len() >= 3,
        "usage: aurora-eval adapter-checks OUT | validate-inputs|run CSV PREFIX OUT [--manifest PATH] [--rcas off|ordinary]"
    );
    if args[1] == "adapter-checks" {
        let out = PathBuf::from(&args[2]);
        ensure!(!out.exists(), "Output must be fresh");
        fs::create_dir_all(&out)?;
        return adapter_checks(&Gpu::new()?, &out);
    }
    ensure!(args.len() >= 5, "CSV, prefix and output required");
    let csv = PathBuf::from(&args[2]);
    let prefix = PathBuf::from(&args[3]);
    let out = PathBuf::from(&args[4]);
    ensure!(!out.exists(), "Output must be fresh");
    fs::create_dir_all(&out)?;
    let mut manifest = csv.parent().unwrap_or(Path::new(".")).join("manifest.json");
    let mut sharpness = None;
    let mut rcas_name = "off";
    let mut index = 5;
    while index < args.len() {
        match args[index].as_str() {
            "--manifest" => {
                index += 1;
                manifest = PathBuf::from(args.get(index).context("manifest value")?);
            }
            "--rcas" => {
                index += 1;
                rcas_name = args.get(index).context("RCAS value")?;
                sharpness = match rcas_name {
                    "off" => None,
                    "ordinary" => Some(contract::ORDINARY_SHARPNESS),
                    _ => bail!("Only off and ordinary arms are permitted"),
                };
            }
            _ => bail!("Unknown option {}", args[index]),
        }
        index += 1;
    }
    let sequence = Sequence::load(&csv, &prefix, &manifest)?;
    if args[1] == "validate-inputs" {
        for frame in &sequence.frames {
            sequence.read(frame)?;
        }
        fs::write(
            out.join("validation.json"),
            serde_json::to_vec_pretty(
                &json!({"frames":sequence.frames.len(),"dimensions":sequence.dimensions,"matrix_convention":contract::MATRIX_CONVENTION,"all_files_valid":true}),
            )?,
        )?;
        return Ok(());
    }
    ensure!(args[1] == "run", "Unknown command");
    let gpu = Gpu::new()?;
    let mut stream = Stream::new(&gpu, sequence.dimensions)?;
    let mut observations = Vec::new();
    let mut rcas_proof = serde_json::Value::Null;
    for frame in &sequence.frames {
        let input = sequence.read(frame)?;
        stream.encode(&gpu, &input, frame, sharpness)?;
        let rgba = half_values(&read_texture(&gpu, stream.fsr.output().texture(), 8)?);
        ensure!(
            rgba.iter().all(|x| x.is_finite()),
            "Nonfinite reconstruction output"
        );
        if frame.index == 0 {
            let internal = half_values(&read_texture(
                &gpu,
                stream.fsr.evaluation_latest_internal().texture(),
                8,
            )?);
            let changed = rgba
                .chunks_exact(4)
                .zip(internal.chunks_exact(4))
                .map(|(a, b)| (0..3).filter(|c| a[*c] != b[*c]).count())
                .sum::<usize>();
            let f = fsr::FrameInputs {
                motion_reproj: frame.reprojection,
                proj: Mat4::from_cols_array_2d(&frame.projection),
                jitter: Vec2::from_array(frame.jitter),
                reset: frame.reset,
                motion_on: true,
                sharpness,
            };
            let effective = fsr::Fsr::evaluation_effective_sharpness(&f);
            ensure!(
                effective == sharpness,
                "Inherited override changed effective RCAS"
            );
            ensure!(
                if effective.is_none() {
                    changed == 0
                } else {
                    changed > 0
                },
                "RCAS output does not match the requested dispatch branch"
            );
            rcas_proof = json!({"effective_sharpness":effective,"dispatched":effective.is_some(),"frame0_final_vs_internal_different_rgb_components":changed});
        }
        save_output(
            &out.join(format!("world-{:03}-output.rgba32f", frame.index)),
            &rgba,
        )?;
        observations.push(
            json!({"frame":frame,"requested_rcas":rcas_name,"rcas_dispatched":sharpness.is_some()}),
        );
    }
    fs::write(
        out.join("run.json"),
        serde_json::to_vec_pretty(
            &json!({"device":gpu.info(),"dimensions":sequence.dimensions,"rcas":rcas_name,"sharpness":sharpness,"rcas_proof":rcas_proof,"opaque_is_final_same_view":true,"matrix_convention":contract::MATRIX_CONVENTION,"display_depth_pass_included":true,"frames":observations,"manifest":sequence.manifest}),
        )?,
    )?;
    Ok(())
}
