use alchemy_aurora_eval::{
    camera_motion, input_motion, pack_motion, projection_contract, view_depth,
};
use glam::{Mat4, Vec2, Vec3, Vec4};
use half::f16;

fn projection() -> Mat4 {
    Mat4::from_cols(
        Vec4::new(0.5, 0., 0., 0.),
        Vec4::Y,
        Vec4::new(0., 0., 0., -1.),
        Vec4::new(0., 0., 0.1, 0.),
    )
}
#[test]
fn packed_motion_is_current_minus_previous_uv_without_extra_y_flip() {
    let p = pack_motion([-4., 2.], 256, 128);
    assert_eq!(
        p,
        [
            f16::ZERO,
            f16::ZERO,
            f16::from_f32(0.015625),
            f16::from_f32(-0.015625)
        ]
    );
    let actual = input_motion(p, Mat4::IDENTITY, Vec2::splat(0.5), 0.05, Vec2::ZERO);
    assert_eq!(actual, Vec2::new(-0.015625, 0.015625));
}
#[test]
fn zero_motion_preserves_actual_camera_fallback_at_near_far_and_clear() {
    let p = projection();
    projection_contract(p).unwrap();
    let r = p * Mat4::from_translation(Vec3::new(-0.1, 0.2, 0.)) * p.inverse();
    for (depth, distance) in [(0.05, 2.), (0.025, 4.)] {
        let actual = input_motion(
            [f16::ZERO; 4],
            r,
            Vec2::new(0.3, 0.4),
            depth,
            Vec2::new(0.2, -0.3) / Vec2::new(256., 128.),
        );
        let expected = Vec2::new(-0.025 / distance, -0.1 / distance);
        assert!(
            (actual - expected).abs().max_element() < 1e-7,
            "{actual:?} vs {expected:?}"
        );
    }
    assert_eq!(
        input_motion([f16::ZERO; 4], r, Vec2::splat(0.5), 0., Vec2::ZERO),
        camera_motion(r, Vec2::splat(0.5), 1e-7, Vec2::ZERO)
    );
}
#[test]
fn identity_does_not_count_projection_jitter_as_surface_motion() {
    let motion = camera_motion(
        Mat4::IDENTITY,
        Vec2::new(0.375, 0.625),
        0.05,
        Vec2::new(0.25, -0.3) / Vec2::new(256., 128.),
    );
    assert!(motion.abs().max_element() < 1e-7);
}
#[test]
fn actual_perspective_projection_recovers_known_view_depth() {
    let p = projection();
    assert!((view_depth(p, 0.05) - 2.).abs() < 1e-4);
    assert!((view_depth(p, 0.025) - 4.).abs() < 1e-4);
    assert!(view_depth(p, 0.).is_finite());
    assert!(projection_contract(Mat4::IDENTITY).is_err());
}
#[test]
fn all_unchanged_upstream_shader_modules_validate_on_cpu() {
    let common = include_str!("../vendor/aurora/shaders/fsr/common.wgsl");
    for pass in [
        include_str!("../vendor/aurora/shaders/fsr/prepare_inputs.wgsl"),
        include_str!("../vendor/aurora/shaders/fsr/pyramids.wgsl"),
        include_str!("../vendor/aurora/shaders/fsr/reactivity.wgsl"),
        include_str!("../vendor/aurora/shaders/fsr/accumulate.wgsl"),
    ] {
        let module = naga::front::wgsl::parse_str(&format!("{common}\n{pass}"))
            .expect("upstream WGSL parses");
        naga::valid::Validator::new(
            naga::valid::ValidationFlags::all(),
            naga::valid::Capabilities::all(),
        )
        .validate(&module)
        .expect("upstream WGSL validates");
    }
}
