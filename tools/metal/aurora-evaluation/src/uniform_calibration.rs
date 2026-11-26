//! A separate conservative bookend envelope for the exact 208-byte wgpu upload.
//! This is NOT a GPU function self-time or an addition to reconstruction time.
use anyhow::{Context, Result, ensure};
use objc2::{rc::Retained, runtime::ProtocolObject};
use objc2_metal::{
    MTLBlitCommandEncoder, MTLBuffer, MTLCommandBuffer, MTLCommandBufferStatus, MTLCommandEncoder,
    MTLCommandQueue, MTLDevice, MTLResourceOptions,
};
use serde_json::json;
use std::{fs, path::PathBuf, time::Instant};
fn marker(
    q: &ProtocolObject<dyn MTLCommandQueue>,
    b: &ProtocolObject<dyn MTLBuffer>,
) -> Result<Retained<ProtocolObject<dyn MTLCommandBuffer>>> {
    let cb = q.commandBuffer().context("Native marker command buffer")?;
    let blit = cb.blitCommandEncoder().context("Native marker blit")?;
    // Valid non-overlapping four-byte copy gives the otherwise empty marker
    // genuine GPU start/end observations. Its work lies outside the envelope.
    unsafe { blit.copyFromBuffer_sourceOffset_toBuffer_destinationOffset_size(b, 0, b, 4, 4) };
    blit.endEncoding();
    cb.commit();
    cb.waitUntilCompleted();
    ensure!(
        cb.status() == MTLCommandBufferStatus::Completed,
        "Native marker failed"
    );
    ensure!(
        cb.GPUStartTime() > 0. && cb.GPUEndTime() > cb.GPUStartTime(),
        "Missing native GPU marker times"
    );
    Ok(cb)
}
fn main() -> Result<()> {
    let args: Vec<_> = std::env::args().collect();
    ensure!(
        args.len() == 2,
        "usage: aurora-uniform-calibration FRESH_OUT"
    );
    let out = PathBuf::from(&args[1]);
    ensure!(!out.exists(), "Output must be fresh");
    ensure!(!cfg!(debug_assertions), "Use optimized release calibration");
    for k in [
        "MTL_DEBUG_LAYER",
        "MTL_SHADER_VALIDATION",
        "WGPU_VALIDATION",
        "WGPU_DEBUG",
    ] {
        ensure!(
            std::env::var(k).map_or(true, |v| v == "0"),
            "Unset debug/validation environment"
        );
    }
    let mut d = wgpu::InstanceDescriptor::new_without_display_handle();
    d.backends = wgpu::Backends::METAL;
    d.flags = wgpu::InstanceFlags::empty();
    let instance = wgpu::Instance::new(d);
    let adapter = pollster::block_on(instance.enumerate_adapters(wgpu::Backends::METAL))
        .into_iter()
        .find(|a| {
            a.get_info().backend == wgpu::Backend::Metal && a.get_info().name.contains("M1 Max")
        })
        .context("Required M1 Max Metal adapter")?;
    let info = adapter.get_info();
    let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
        label: Some("Uniform transfer conservative envelope"),
        ..Default::default()
    }))?;
    let raw_device =
        unsafe { device.as_hal::<wgpu::hal::api::Metal>() }.context("Metal device observation")?;
    let raw_queue =
        unsafe { queue.as_hal::<wgpu::hal::api::Metal>() }.context("Metal queue observation")?;
    let b = raw_device
        .raw_device()
        .newBufferWithLength_options(16, MTLResourceOptions::StorageModeShared)
        .context("Marker storage")?;
    unsafe { std::ptr::write_bytes(b.contents().as_ptr(), 0, 16) };
    let uniform = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("Exact source-defined FSR argument size"),
        size: 208,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });
    let cpu_epoch = Instant::now();
    let mut samples = Vec::new();
    for i in 0..80 {
        let before = marker(raw_queue.as_raw(), &b)?;
        let write_begin = cpu_epoch.elapsed().as_secs_f64();
        let bytes = [(i & 255) as u8; 208];
        queue.write_buffer(&uniform, 0, &bytes);
        let submission = queue.submit([]);
        let submit_end = cpu_epoch.elapsed().as_secs_f64();
        // Waiting here makes the bound conservative and proves the actual
        // hidden transfer is wholly between the two native GPU bookends.
        device.poll(wgpu::PollType::Wait {
            submission_index: Some(submission),
            timeout: None,
        })?;
        let completed = cpu_epoch.elapsed().as_secs_f64();
        let after = marker(raw_queue.as_raw(), &b)?;
        let span = (after.GPUStartTime() - before.GPUEndTime()) * 1000.;
        ensure!(span > 0., "Bookends not chronologically ordered");
        if i >= 16 {
            samples.push(json!({"sample":i-16,"before_gpu_start_seconds":before.GPUStartTime(),"before_gpu_end_seconds":before.GPUEndTime(),"after_gpu_start_seconds":after.GPUStartTime(),"after_gpu_end_seconds":after.GPUEndTime(),"observed_copy_plus_host_and_scheduling_upper_envelope_ms":span,"cpu_write_begin_relative_seconds":write_begin,"cpu_submit_end_relative_seconds":submit_end,"cpu_copy_completion_observed_relative_seconds":completed}));
        }
    }
    let mut sorted: Vec<_> = samples
        .iter()
        .map(|v| {
            v["observed_copy_plus_host_and_scheduling_upper_envelope_ms"]
                .as_f64()
                .unwrap()
        })
        .collect();
    sorted.sort_by(f64::total_cmp);
    let result = json!({"scope":"ONE bounded exact-API transfer calibration,64samples after16warm; NOT isolated GPU self-time, NOT directly additive to reconstruction","device":info.name,"backend":"Metal","bytes_per_write":208,"api":"wgpu::Queue::write_buffer208B then submit([]) and pollcompletion","order":"native four-byte markerA commit/wait; exact wgpu upload/submit/poll; markerB commit/wait","proof":"Both marker GPU times are the same native clock. Completion is observed before submitting markerB; actual transfer occurs wholly within A.GPUEnd→B.GPUStart. Deliberate host waits, upload CPU staging, callback and driver/scheduling gaps are INCLUDED, making this an observed conservative envelope only, not a general worst-case guarantee.","sample_count":64,"warm_count":16,"summary":{"min_ms":sorted[0],"median_ms":(sorted[31]+sorted[32])/2.,"p95_ms":sorted[60],"max_ms":sorted[63]},"samples":samples});
    fs::create_dir_all(&out)?;
    fs::write(
        out.join("uniform-calibration.json"),
        serde_json::to_vec_pretty(&result)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&result["summary"])?);
    Ok(())
}
