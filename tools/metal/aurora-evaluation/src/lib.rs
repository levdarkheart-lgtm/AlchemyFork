//! CPU-only input contract for the isolated, pinned Aurora reconstruction tool.
use anyhow::{Context, Result, bail, ensure};
use glam::{Mat4, Vec2, Vec4};
use half::f16;
use serde::Serialize;
use std::{
    collections::HashMap,
    fs,
    path::{Path, PathBuf},
};

pub const MATRIX_CONVENTION: &str = "aurora_uv_y_down_ndc_y_up_column_major";
pub const ORDINARY_SHARPNESS: f32 = 0.8;
#[derive(Clone, Copy, Debug, Serialize, PartialEq, Eq)]
pub struct Dimensions {
    pub width: u32,
    pub height: u32,
    pub output_width: u32,
    pub output_height: u32,
}
#[derive(Clone, Debug, Serialize)]
pub struct Frame {
    pub index: usize,
    pub world_id: u64,
    pub phase: u32,
    pub jitter: [f32; 2],
    pub reset: bool,
    pub projection: [[f32; 4]; 4],
    pub reprojection: [[f32; 4]; 4],
}
#[derive(Debug)]
pub struct Sequence {
    pub dimensions: Dimensions,
    pub frames: Vec<Frame>,
    pub prefix: PathBuf,
    pub manifest: serde_json::Value,
}
pub struct Inputs {
    pub color: Vec<[f16; 4]>,
    pub depth: Vec<f32>,
    pub motion_pixels: Vec<[f16; 2]>,
}

pub fn pack_motion(pixels: [f32; 2], width: u32, height: u32) -> [f16; 4] {
    [
        f16::ZERO,
        f16::ZERO,
        f16::from_f32(-pixels[0] / width as f32),
        f16::from_f32(-pixels[1] / height as f32),
    ]
}
pub fn camera_motion(reprojection: Mat4, uv: Vec2, depth: f32, jitter_uv: Vec2) -> Vec2 {
    let current = uv - jitter_uv;
    let previous = reprojection
        * Vec4::new(
            2. * current.x - 1.,
            1. - 2. * current.y,
            depth.max(1e-7),
            1.,
        );
    if previous.w <= 0. {
        return Vec2::splat(2.);
    }
    Vec2::new(
        previous.x / previous.w * 0.5 + 0.5,
        0.5 - previous.y / previous.w * 0.5,
    ) - current
}
pub fn input_motion(
    packed: [f16; 4],
    reprojection: Mat4,
    uv: Vec2,
    depth: f32,
    jitter_uv: Vec2,
) -> Vec2 {
    let motion = Vec2::new(packed[2].to_f32(), packed[3].to_f32());
    if depth > 0. && motion != Vec2::ZERO {
        -motion
    } else {
        camera_motion(reprojection, uv, depth, jitter_uv)
    }
}
pub fn view_depth(projection: Mat4, depth: f32) -> f32 {
    projection.w_axis.z / (depth - (-projection.z_axis.z - f32::EPSILON))
}
pub fn projection_contract(projection: Mat4) -> Result<()> {
    ensure!(
        projection.is_finite() && projection.w_axis.w == 0. && projection.z_axis.w < 0.,
        "Expected actual perspective projection, not old orthographic fixture"
    );
    ensure!(
        projection.x_axis.x > 0. && projection.y_axis.y > 0. && projection.w_axis.z > 0.,
        "Expected NDC Y-up reverse-Z projection"
    );
    let near = projection * Vec4::new(0., 0., -2., 1.);
    let far = projection * Vec4::new(0., 0., -4., 1.);
    ensure!(
        near.z / near.w > far.z / far.w && far.z / far.w > 0.,
        "Projection depth does not use reverse-Z"
    );
    Ok(())
}

fn csv_row(line: &str) -> Result<Vec<&str>> {
    ensure!(
        !line.contains('"'),
        "Only the producer's numeric unquoted CSV is accepted"
    );
    Ok(line.trim_end_matches('\r').split(',').collect())
}
impl Sequence {
    pub fn load(csv: &Path, prefix: &Path, manifest: &Path) -> Result<Self> {
        let manifest: serde_json::Value =
            serde_json::from_slice(&fs::read(manifest).context("Read input manifest")?)?;
        ensure!(
            manifest["matrix_convention"] == MATRIX_CONVENTION,
            "Unexpected/missing matrix convention"
        );
        ensure!(
            manifest["opaque_equals_final"] == true && manifest["composition_marker"] == 0,
            "This MASK-only adapter requires opaque==final and static composition marker0"
        );
        ensure!(
            manifest["depth_reversed"] == true && manifest["pre_exposure"] == 1,
            "Manifest depth/exposure contract mismatch"
        );
        let text = fs::read_to_string(csv)?;
        let mut lines = text.lines();
        let header = csv_row(lines.next().context("Empty CSV")?)?;
        let keys: HashMap<_, _> = header
            .iter()
            .enumerate()
            .map(|(i, name)| (*name, i))
            .collect();
        ensure!(keys.len() == header.len(), "Duplicate CSV fields");
        let mut frames = Vec::new();
        let mut dimensions = None;
        for line in lines {
            let values = csv_row(line)?;
            ensure!(values.len() == header.len(), "Incomplete metadata row");
            let field = |key: &str| -> Result<&str> {
                Ok(values[*keys
                    .get(key)
                    .with_context(|| format!("Missing field {key}"))?])
            };
            let integer = |key: &str| -> Result<u32> {
                Ok(field(key)?
                    .parse()
                    .with_context(|| format!("Integer {key}"))?)
            };
            let number = |key: &str| -> Result<f32> {
                let n: f32 = field(key)?.parse()?;
                ensure!(n.is_finite(), "Nonfinite {key}");
                Ok(n)
            };
            let dims = Dimensions {
                width: integer("input_width")?,
                height: integer("input_height")?,
                output_width: integer("output_width")?,
                output_height: integer("output_height")?,
            };
            ensure!(
                dims.width > 0
                    && dims.height > 0
                    && dims.width == dims.output_width.saturating_mul(2).div_ceil(3)
                    && dims.height == dims.output_height.saturating_mul(2).div_ceil(3),
                "Only actual rounded2/3 input dimensions are accepted"
            );
            ensure!(
                dimensions.is_none_or(|old| old == dims),
                "Descriptor changed inside one history stream"
            );
            dimensions = Some(dims);
            let index = frames.len();
            ensure!(
                integer("frame")? as usize == index && integer("world_id")? as usize == index + 1,
                "Missing/reordered world"
            );
            ensure!(
                integer("phase")? as usize == index % 18 && integer("phase_count")? == 18,
                "Unexpected production jitter phase"
            );
            ensure!(
                integer("reset")? == u32::from(index == 0)
                    && integer("depth_reversed")? == 1
                    && number("pre_exposure")? == 1.,
                "Reset/depth/exposure contract mismatch"
            );
            let matrix = |prefix: &str| -> Result<[[f32; 4]; 4]> {
                let mut columns = [[0.; 4]; 4];
                for c in 0..4 {
                    for r in 0..4 {
                        columns[c][r] = number(&format!("{prefix}_{}", c * 4 + r))?;
                    }
                }
                Ok(columns)
            };
            let projection = matrix("projection")?;
            projection_contract(Mat4::from_cols_array_2d(&projection))?;
            let reprojection = matrix("reprojection")?;
            ensure!(
                Mat4::from_cols_array_2d(&reprojection).is_finite(),
                "Nonfinite reprojection"
            );
            let jitter = [number("jitter_x")?, number("jitter_y")?];
            ensure!(
                jitter.iter().all(|x| x.abs() <= 0.5),
                "Jitter outside input-pixel bounds"
            );
            frames.push(Frame {
                index,
                world_id: index as u64 + 1,
                phase: integer("phase")?,
                jitter,
                reset: index == 0,
                projection,
                reprojection,
            });
        }
        let count = |key: &str| -> Result<u64> {
            let value = manifest[key]
                .as_u64()
                .with_context(|| format!("Missing manifest count {key}"))?;
            ensure!(
                value > 0 && value % 18 == 0,
                "Manifest {key} must contain complete18-phase cycles"
            );
            Ok(value)
        };
        let main_count = count("warm_frames")?
            + count("stable_frames")?
            + count("moving_frames")?
            + count("stopped_frames")?;
        let reference_count = count("reference_frames")?;
        ensure!(
            reference_count < main_count,
            "Reference/main lengths must be distinct"
        );
        ensure!(
            frames.len() as u64 == main_count || frames.len() as u64 == reference_count,
            "CSV does not contain the manifest's complete main or reference history: {} not {main_count}/{reference_count}",
            frames.len()
        );
        let prefix = if prefix.is_absolute() {
            prefix.to_owned()
        } else {
            csv.parent().unwrap_or(Path::new(".")).join(prefix)
        };
        Ok(Self {
            dimensions: dimensions.context("No worlds")?,
            frames,
            prefix,
            manifest,
        })
    }
    pub fn read(&self, frame: &Frame) -> Result<Inputs> {
        let base = format!("{}{:03}", self.prefix.display(), frame.index);
        let count = self.dimensions.width as usize * self.dimensions.height as usize;
        let color_raw = fs::read(format!("{base}-input.rgba32f"))?;
        ensure!(color_raw.len() == count * 16, "Wrong color byte count");
        let mut color = Vec::with_capacity(count);
        for pixel in color_raw.chunks_exact(16) {
            let mut packed = [f16::ZERO; 4];
            for (c, bytes) in pixel.chunks_exact(4).enumerate() {
                let value = f32::from_le_bytes(bytes.try_into().unwrap());
                packed[c] = f16::from_f32(value);
                ensure!(
                    value.is_finite() && packed[c].to_f32() == value,
                    "Color is not an exact finite half expansion"
                );
            }
            color.push(packed);
        }
        let depth_raw = fs::read(format!("{base}-proxy-depth.r32f"))?;
        ensure!(depth_raw.len() == count * 4, "Wrong depth byte count");
        let depth: Vec<f32> = depth_raw
            .chunks_exact(4)
            .map(|x| f32::from_le_bytes(x.try_into().unwrap()))
            .collect();
        ensure!(
            depth
                .iter()
                .all(|x| x.is_finite() && (0. ..=1.).contains(x)),
            "Invalid reverse-Z depth"
        );
        let motion_raw = fs::read(format!("{base}-motion.rg16f"))?;
        ensure!(motion_raw.len() == count * 4, "Wrong motion byte count");
        let mut motion_pixels = Vec::with_capacity(count);
        for pixel in motion_raw.chunks_exact(4) {
            let pair = [
                f16::from_bits(u16::from_le_bytes(pixel[0..2].try_into().unwrap())),
                f16::from_bits(u16::from_le_bytes(pixel[2..4].try_into().unwrap())),
            ];
            ensure!(
                pair.iter().all(|x| x.is_finite()),
                "Nonfinite source motion"
            );
            motion_pixels.push(pair);
        }
        Ok(Inputs {
            color,
            depth,
            motion_pixels,
        })
    }
}
pub fn reject_overrides() -> Result<()> {
    for key in [
        "AURORA_FSR_SHARPNESS",
        "AURORA_FSR_REACTIVE",
        "AURORA_FSR_COMPOSITION",
        "AURORA_FSR_MIP_BIAS",
    ] {
        if std::env::var_os(key).is_some() {
            bail!("Remove inherited {key}; requested arms must use exact pinned defaults");
        }
    }
    Ok(())
}
