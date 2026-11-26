// Aurora's cache is a no-op on Metal. Keep its existing call surface without
// adding the viewer's Vulkan disk-cache/thread subsystem to this Metal tool.
pub trait CachedPipelines {
    fn create_render_pipeline_cached(
        &self,
        descriptor: &wgpu::RenderPipelineDescriptor<'_>,
    ) -> wgpu::RenderPipeline;
    fn create_compute_pipeline_cached(
        &self,
        descriptor: &wgpu::ComputePipelineDescriptor<'_>,
    ) -> wgpu::ComputePipeline;
}
impl CachedPipelines for wgpu::Device {
    fn create_render_pipeline_cached(
        &self,
        descriptor: &wgpu::RenderPipelineDescriptor<'_>,
    ) -> wgpu::RenderPipeline {
        self.create_render_pipeline(descriptor)
    }
    fn create_compute_pipeline_cached(
        &self,
        descriptor: &wgpu::ComputePipelineDescriptor<'_>,
    ) -> wgpu::ComputePipeline {
        self.create_compute_pipeline(descriptor)
    }
}
