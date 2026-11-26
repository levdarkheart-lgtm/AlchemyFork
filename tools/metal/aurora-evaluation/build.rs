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
    let vendor = root.join("vendor/aurora");
    let original = fs::read_to_string(vendor.join("fsr.rs")).unwrap();
    // Verbatim upstream prefix. Read-only host observations do not change a
    // pass, parameter, shader, dispatch or accumulation decision.
    let accessors = r#"
impl Fsr {
    pub(crate) fn evaluation_dilated_motion(&self) -> &wgpu::TextureView { &self.dilated_motion }
    pub(crate) fn evaluation_generated_reactive(&self) -> &wgpu::TextureView { &self.reactive }
    pub(crate) fn evaluation_latest_internal(&self) -> &wgpu::TextureView { &self.internal[1-self.parity] }
    pub(crate) fn evaluation_effective_sharpness(f: &FrameInputs) -> Option<f32> { sharpness_override().unwrap_or(f.sharpness) }
}
"#;
    let generated = output.join("fsr_observed.rs");
    fs::write(&generated, format!("{original}{accessors}")).unwrap();
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
