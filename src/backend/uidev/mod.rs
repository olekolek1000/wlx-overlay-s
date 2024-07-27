use std::sync::Arc;

use vulkano::{
    command_buffer::CommandBufferUsage,
    image::{view::ImageView, ImageUsage},
    swapchain::{
        acquire_next_image, Surface, Swapchain, SwapchainCreateInfo, SwapchainPresentInfo,
    },
    sync::GpuFuture,
    Validated, VulkanError,
};
use winit::{
    dpi::LogicalSize,
    event::{Event, WindowEvent},
    event_loop::ControlFlow,
    window::Window,
};

use crate::{
    graphics::{DynamicPass, DynamicPipeline, WlxGraphics, BLEND_ALPHA},
    hid::USE_UINPUT,
    overlays::cef::create_cef,
    state::{AppState, ScreenMeta},
};

use super::{
    input::{TrackedDevice, TrackedDeviceRole},
    overlay::{OverlayBackend, OverlayData},
};

static LAST_SIZE: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);

struct PreviewState {
    backend: Box<dyn OverlayBackend>,
    pipeline: Arc<DynamicPipeline>,
    pass: DynamicPass,
    swapchain: Arc<Swapchain>,
    images: Vec<Arc<ImageView>>,
}

#[derive(Default)]
pub struct StubOverlayData {}

impl PreviewState {
    fn new(
        state: &mut AppState,
        surface: Arc<Surface>,
        window: Arc<Window>,
    ) -> anyhow::Result<Self> {
        let last_size = {
            let size_u64 = LAST_SIZE.load(std::sync::atomic::Ordering::Relaxed);
            [size_u64 as u32, (size_u64 >> 32) as u32]
        };

        let preview_size: [u32; 2] = [1280, 720];

        let logical_size = LogicalSize::new(preview_size[0], preview_size[1]);
        let _ = window.request_inner_size(logical_size);
        window.set_min_inner_size(Some(logical_size));
        window.set_max_inner_size(Some(logical_size));
        LAST_SIZE.store(
            (preview_size[1] as u64) << 32 | preview_size[0] as u64,
            std::sync::atomic::Ordering::Relaxed,
        );

        let inner_size = window.inner_size();
        let swapchain_size = [inner_size.width, inner_size.height];
        let (swapchain, images) =
            create_swapchain(&state.graphics, surface.clone(), swapchain_size)?;

        let mut cef: OverlayData<StubOverlayData> = create_cef(state)?;

        let view = cef.backend.view().unwrap();

        let pipeline = {
            let shaders = state.graphics.shared_shaders.read().unwrap();
            state.graphics.create_pipeline_dynamic(
                shaders.get("vert_common").unwrap().clone(), // want panic
                shaders.get("frag_sprite").unwrap().clone(), // want panic
                state.graphics.native_format,
                Some(BLEND_ALPHA),
            )
        }?;
        let set0 = pipeline
            .uniform_sampler(0, view.clone(), pipeline.graphics.texture_filtering)
            .unwrap();

        let pass = pipeline
            .create_pass(
                [swapchain_size[0] as f32, swapchain_size[1] as f32],
                state.graphics.quad_verts.clone(),
                state.graphics.quad_indices.clone(),
                vec![set0],
            )
            .unwrap();

        Ok(PreviewState {
            backend: cef.backend,
            pipeline,
            pass,
            swapchain,
            images,
        })
    }
}

pub fn uidev_run(_panel_name: &str) -> anyhow::Result<()> {
    let (graphics, event_loop, window, surface) = WlxGraphics::new_window()?;
    window.set_resizable(false);
    window.set_title("WlxOverlay CEF Preview");

    USE_UINPUT.store(false, std::sync::atomic::Ordering::Relaxed);

    let mut state = AppState::from_graphics(graphics.clone())?;
    add_dummy_devices(&mut state);
    add_dummy_screens(&mut state);

    let mut preview = Some(PreviewState::new(
        &mut state,
        surface.clone(),
        window.clone(),
    )?);

    let mut recreate = false;
    let mut last_draw = std::time::Instant::now();

    event_loop.run(move |event, elwt| {
        elwt.set_control_flow(ControlFlow::Poll);

        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested,
                ..
            } => {
                elwt.exit();
            }
            Event::WindowEvent {
                event: WindowEvent::RedrawRequested,
                ..
            } => {
                if recreate {
                    drop(preview.take());
                    preview = Some(
                        PreviewState::new(&mut state, surface.clone(), window.clone()).unwrap(),
                    );
                    recreate = false;
                    window.request_redraw();
                }

                {
                    let preview = preview.as_mut().unwrap();
                    let (image_index, _, acquire_future) =
                        match acquire_next_image(preview.swapchain.clone(), None)
                            .map_err(Validated::unwrap)
                        {
                            Ok(r) => r,
                            Err(VulkanError::OutOfDate) => {
                                recreate = true;
                                return;
                            }
                            Err(e) => panic!("failed to acquire next image: {e}"),
                        };

                    if let Err(e) = preview.backend.render(&mut state) {
                        log::error!("failed to render canvas: {e}");
                        window.request_redraw();
                    }

                    let target = preview.images[image_index as usize].clone();

                    let mut cmd_buf = state
                        .graphics
                        .create_command_buffer(CommandBufferUsage::OneTimeSubmit)
                        .unwrap();
                    cmd_buf.begin_rendering(target).unwrap();
                    cmd_buf.run_ref(&preview.pass).unwrap();
                    cmd_buf.end_rendering().unwrap();
                    last_draw = std::time::Instant::now();

                    let command_buffer = cmd_buf.build().unwrap();
                    vulkano::sync::now(graphics.device.clone())
                        .join(acquire_future)
                        .then_execute(graphics.queue.clone(), command_buffer)
                        .unwrap()
                        .then_swapchain_present(
                            graphics.queue.clone(),
                            SwapchainPresentInfo::swapchain_image_index(
                                preview.swapchain.clone(),
                                image_index,
                            ),
                        )
                        .then_signal_fence_and_flush()
                        .unwrap()
                        .wait(None)
                        .unwrap();
                }
            }
            Event::AboutToWait => {
                if last_draw.elapsed().as_millis() > 100 {
                    window.request_redraw();
                }
            }
            _ => (),
        }
    })?;

    Ok(())
}

fn create_swapchain(
    graphics: &WlxGraphics,
    surface: Arc<Surface>,
    extent: [u32; 2],
) -> anyhow::Result<(Arc<Swapchain>, Vec<Arc<ImageView>>)> {
    let surface_capabilities = graphics
        .device
        .physical_device()
        .surface_capabilities(&surface, Default::default())
        .unwrap();

    let (swapchain, images) = Swapchain::new(
        graphics.device.clone(),
        surface.clone(),
        SwapchainCreateInfo {
            min_image_count: surface_capabilities.min_image_count.max(2),
            image_format: graphics.native_format,
            image_extent: extent,
            image_usage: ImageUsage::COLOR_ATTACHMENT,
            composite_alpha: surface_capabilities
                .supported_composite_alpha
                .into_iter()
                .next()
                .unwrap(),
            ..Default::default()
        },
    )?;

    let image_views = images
        .into_iter()
        .map(|image| ImageView::new_default(image).unwrap())
        .collect::<Vec<_>>();

    Ok((swapchain, image_views))
}

fn add_dummy_devices(app: &mut AppState) {
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::Hmd,
        soc: Some(0.42),
        charging: true,
    });
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::LeftHand,
        soc: Some(0.72),
        charging: false,
    });
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::RightHand,
        soc: Some(0.73),
        charging: false,
    });
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::Tracker,
        soc: Some(0.65),
        charging: false,
    });
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::Tracker,
        soc: Some(0.67),
        charging: false,
    });
    app.input_state.devices.push(TrackedDevice {
        role: TrackedDeviceRole::Tracker,
        soc: Some(0.69),
        charging: false,
    });
}

fn add_dummy_screens(app: &mut AppState) {
    app.screens.push(ScreenMeta {
        name: "HDMI-A-1".into(),
        id: 0,
        native_handle: 0,
    });
    app.screens.push(ScreenMeta {
        name: "DP-2".into(),
        id: 0,
        native_handle: 0,
    });
    app.screens.push(ScreenMeta {
        name: "DP-3".into(),
        id: 0,
        native_handle: 0,
    });
}
