use std::{env, fs, path::PathBuf, process::Command};

fn main() {
    let rustc = env::var("RUSTC").unwrap_or_else(|_| "rustc".into());
    let version = Command::new(&rustc)
        .arg("--version")
        .output()
        .expect("rustc version");
    let version = String::from_utf8(version.stdout).expect("UTF8 rustc version");
    assert!(
        version.starts_with("rustc 1.99.0 "),
        "Use the pinned Rust1.99.0 compiler, observed {version}"
    );
    println!("cargo:rustc-env=AURORA_EVAL_RUSTC={}", version.trim());
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap());

    let object = output.join("input_workload.o");
    let archive = output.join("libae_workload.a");
    let status = Command::new("/usr/bin/clang")
        .args([
            "-std=c11",
            "-O3",
            "-ffp-contract=off",
            "-arch",
            "arm64",
            "-c",
        ])
        .arg(root.join("workload/input_workload.c"))
        .arg("-o")
        .arg(&object)
        .status()
        .expect("Build shared producer");
    assert!(status.success(), "Shared input producer compilation failed");
    let status = Command::new("/usr/bin/ar")
        .arg("rcs")
        .arg(&archive)
        .arg(&object)
        .status()
        .expect("Archive shared producer");
    assert!(status.success(), "Shared input producer archive failed");
    println!("cargo:rustc-link-search=native={}", output.display());
    println!("cargo:rustc-link-lib=static=ae_workload");
    println!("cargo:rerun-if-changed=workload/input_workload.c");
    println!("cargo:rerun-if-changed=workload/input_workload.h");
    let vendor = root.join("vendor/aurora");
    let original = fs::read_to_string(vendor.join("fsr.rs")).unwrap();
    // The pinned vendor source stays verbatim. Read-only observations and
    // optional host timing descriptors change no reconstruction decision.
    let accessors = r#"
impl Fsr {
    pub(crate) fn evaluation_dilated_motion(&self) -> &wgpu::TextureView { &self.dilated_motion }
    pub(crate) fn evaluation_generated_reactive(&self) -> &wgpu::TextureView { &self.reactive }
    pub(crate) fn evaluation_latest_internal(&self) -> &wgpu::TextureView { &self.internal[1-self.parity] }
    pub(crate) fn evaluation_owned_textures(&self) -> Vec<(&'static str, &wgpu::Texture)> {
        let mut result=vec![("reactive",self.reactive.texture()),("dilated_motion",self.dilated_motion.texture()),("dilated_depth",self.dilated_depth.texture()),("farthest",self.farthest.texture()),("luma0",self.luma[0].texture()),("luma1",self.luma[1].texture()),("farthest_mip1",self.farthest_mip1.texture()),("shading_change",self.shading_change.texture()),("masks",self.masks.texture()),("accumulation0",self.accumulation[0].texture()),("accumulation1",self.accumulation[1].texture()),("luma_history0",self.luma_history[0].texture()),("luma_history1",self.luma_history[1].texture()),("instability",self.instability.texture()),("internal0",self.internal[0].texture()),("internal1",self.internal[1].texture()),("output",self.output.texture())];
        if let Some(v)=self.spd_levels.first(){result.push(("shading_pyramid",v.texture()));}
        if let Some(v)=&self.display_depth{result.push(("display_depth",v.texture()));}
        result
    }
    pub(crate) fn evaluation_owned_buffer_payload(&self) -> (u64,u64,u64) { (self.buffer.size(),self.reconstructed.size(),u64::from(self.display.0)*u64::from(self.display.1)*4) }
    pub(crate) fn evaluation_effective_sharpness(f: &FrameInputs) -> Option<f32> { sharpness_override().unwrap_or(f.sharpness) }
}
"#;
    // Only two host render-pass descriptors gain optional stage timestamps.
    // The vendored module, shaders, pass sequence and algorithm remain untouched.
    let mut observed = original.clone();
    for (anchor, replacement) in [
        (
            "if let Some(depth) = &self.display_depth {",
            "timestamp_writes: crate::benchmark::pass_timing(\"fsr display depth\"),",
        ),
        (
            "fn run(\n",
            "timestamp_writes: crate::benchmark::pass_timing(label),",
        ),
    ] {
        let start = observed.find(anchor).expect("Pinned host timing anchor");
        let at = start
            + observed[start..]
                .find("timestamp_writes: None,")
                .expect("Pinned timestamp field");
        observed.replace_range(at..at + "timestamp_writes: None,".len(), replacement);
    }
    let generated = output.join("fsr_observed.rs");
    fs::write(&generated, format!("{observed}{accessors}")).unwrap();
    fs::write(output.join("host-timing-adaptations.txt"), "Only run() timestamp_writes and display_depth timestamp_writes use optional benchmark stage-boundary queries. All other vendor host expressions and every WGSL byte are unchanged.\n").unwrap();
    fs::write(
        output.join("aurora_module.rs"),
        format!("#[path = {:?}]\nmod fsr;\n", generated),
    )
    .unwrap();
    fs::write(output.join("host-observation-append.rs"), accessors).unwrap();
    let shaders = output.join("shaders/fsr");
    fs::create_dir_all(&shaders).unwrap();
    for entry in fs::read_dir(vendor.join("shaders/fsr")).unwrap() {
        let entry = entry.unwrap();
        if entry.path().extension().is_some_and(|x| x == "wgsl") {
            fs::copy(entry.path(), shaders.join(entry.file_name())).unwrap();
            println!("cargo:rerun-if-changed={}", entry.path().display());
        }
    }
    println!("cargo:rerun-if-changed=vendor/aurora/fsr.rs");
    println!("cargo:rerun-if-changed=build.rs");
}
