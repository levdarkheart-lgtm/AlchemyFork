use std::sync::atomic::{AtomicU64, Ordering};
static CREATED_TEXTURES: AtomicU64 = AtomicU64::new(0);
pub(crate) fn created_texture_count() -> u64 {
    CREATED_TEXTURES.load(Ordering::Relaxed)
}
// The upstream allocation helper, with COPY_SRC added only to the final output
// and two explicitly observed diagnostic inputs. No filter/format changes.
pub(crate) fn make_view(
    device: &wgpu::Device,
    label: &str,
    width: u32,
    height: u32,
    format: wgpu::TextureFormat,
    samples: u32,
    mut usage: wgpu::TextureUsages,
) -> (wgpu::Texture, wgpu::TextureView) {
    if matches!(
        label,
        "fsr output" | "fsr dilated motion" | "fsr reactive mask" | "fsr internal colour"
    ) {
        usage |= wgpu::TextureUsages::COPY_SRC;
    }
    CREATED_TEXTURES.fetch_add(1, Ordering::Relaxed);
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some(label),
        size: wgpu::Extent3d {
            width: width.max(1),
            height: height.max(1),
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: samples,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage,
        view_formats: &[],
    });
    let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
    (texture, view)
}
