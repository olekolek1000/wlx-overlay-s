use glam::{vec2, vec3a, Affine2, Vec2};
use std::{cell::RefCell, rc::Rc, sync::Arc};
use vulkano::image::SubresourceLayout;
use wayvr::wayvr;
use wlx_capture::frame::{DmabufFrame, FourCC, FrameFormat, FramePlane};

use crate::{
    backend::{
        input::{self, InteractionHandler},
        overlay::{OverlayData, OverlayRenderer, OverlayState, SplitOverlayBackend},
    },
    graphics::WlxGraphics,
    state,
};

pub struct WayVRContext {
    wayvr: wayvr::WayVR,
}

impl WayVRContext {
    pub fn new(width: u32, height: u32) -> anyhow::Result<Self> {
        let mut wayvr = wayvr::WayVR::new(width, height)?;

        //wayvr.spawn_process("thunar", vec![""], vec![])?;
        //wayvr.spawn_process("kcalc", vec![], vec![])?;
        //wayvr.spawn_process("weston-smoke", vec![], vec![])?;
        wayvr.spawn_process(
            "cage",
            vec![
                "chromium",
                "--",
                "--incognito",
                "--ozone-platform=wayland",
                "--start-maximized",
                "https://www.shadertoy.com/",
            ],
            vec![],
        )?;

        Ok(Self { wayvr })
    }
}

pub struct WayVRInteractionHandler {
    context: Rc<RefCell<WayVRContext>>,
    mouse_transform: Affine2,
}

impl WayVRInteractionHandler {
    pub fn new(context: Rc<RefCell<WayVRContext>>, mouse_transform: Affine2) -> Self {
        Self {
            context,
            mouse_transform,
        }
    }
}

impl InteractionHandler for WayVRInteractionHandler {
    fn on_hover(
        &mut self,
        _app: &mut state::AppState,
        hit: &input::PointerHit,
    ) -> Option<input::Haptics> {
        let mut ctx = self.context.borrow_mut();

        let pos = self.mouse_transform.transform_point2(hit.uv);
        let x = (pos.x as i32).max(0);
        let y = (pos.y as i32).max(0);

        ctx.wayvr.send_mouse_move(x as u32, y as u32);

        None
    }

    fn on_left(&mut self, _app: &mut state::AppState, _pointer: usize) {
        // Ignore event
    }

    fn on_pointer(&mut self, _app: &mut state::AppState, hit: &input::PointerHit, pressed: bool) {
        if let Some(index) = match hit.mode {
            input::PointerMode::Left => Some(wayvr::MouseIndex::Left),
            input::PointerMode::Middle => Some(wayvr::MouseIndex::Center),
            input::PointerMode::Right => Some(wayvr::MouseIndex::Right),
            _ => {
                // Unknown pointer event, ignore
                None
            }
        } {
            let mut ctx = self.context.borrow_mut();
            if pressed {
                ctx.wayvr.send_mouse_down(index);
            } else {
                ctx.wayvr.send_mouse_up(index);
            }
        }
    }

    fn on_scroll(&mut self, _app: &mut state::AppState, _hit: &input::PointerHit, delta: f32) {
        let mut ctx = self.context.borrow_mut();
        ctx.wayvr.send_mouse_scroll(delta);
    }
}

pub struct WayVRRenderer {
    dmabuf_image: Option<Arc<vulkano::image::Image>>,
    view: Option<Arc<vulkano::image::view::ImageView>>,
    context: Rc<RefCell<WayVRContext>>,
    graphics: Arc<WlxGraphics>,
    width: u32,
    height: u32,
}

impl WayVRRenderer {
    pub fn new(app: &mut state::AppState, width: u32, height: u32) -> anyhow::Result<Self> {
        Ok(Self {
            context: Rc::new(RefCell::new(WayVRContext::new(width, height)?)),
            width,
            height,
            dmabuf_image: None,
            view: None,
            graphics: app.graphics.clone(),
        })
    }
}

impl WayVRRenderer {
    fn ensure_dmabuf(&mut self, data: wayvr::egl_data::DMAbufData) -> anyhow::Result<()> {
        if self.dmabuf_image.is_none() {
            // First init
            let mut planes = [FramePlane::default(); 4];
            planes[0].fd = Some(data.fd);
            planes[0].offset = data.offset as u32;
            planes[0].stride = data.stride;

            let frame = DmabufFrame {
                format: FrameFormat {
                    width: self.width,
                    height: self.height,
                    fourcc: FourCC {
                        value: data.mod_info.fourcc,
                    },
                    modifier: data.mod_info.modifiers[0], /* possibly not proper? */
                },
                num_planes: 1,
                planes,
            };

            let layouts: Vec<SubresourceLayout> = vec![SubresourceLayout {
                offset: data.offset as _,
                size: 0,
                row_pitch: data.stride as _,
                array_pitch: None,
                depth_pitch: None,
            }];

            let tex = self.graphics.dmabuf_texture_ex(
                frame,
                vulkano::image::ImageTiling::DrmFormatModifier,
                layouts,
                data.mod_info.modifiers,
            )?;
            self.dmabuf_image = Some(tex.clone());
            self.view = Some(vulkano::image::view::ImageView::new_default(tex).unwrap());
        }

        Ok(())
    }
}

impl OverlayRenderer for WayVRRenderer {
    fn init(&mut self, _app: &mut state::AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn pause(&mut self, _app: &mut state::AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn resume(&mut self, _app: &mut state::AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn render(&mut self, _app: &mut state::AppState) -> anyhow::Result<()> {
        let mut ctx = self.context.borrow_mut();
        // Process, tick and render Wayland message loop
        ctx.wayvr.tick()?;

        let dmabuf_data = ctx.wayvr.get_dmabuf_data();
        drop(ctx);
        self.ensure_dmabuf(dmabuf_data.clone())?;

        Ok(())
    }

    fn view(&mut self) -> Option<Arc<vulkano::image::view::ImageView>> {
        self.view.clone()
    }

    fn extent(&mut self) -> Option<[u32; 3]> {
        self.view.as_ref().map(|view| view.image().extent())
    }
}

pub fn create_wayvr<O>(
    app: &mut state::AppState,
    width: u32,
    height: u32,
) -> anyhow::Result<OverlayData<O>>
where
    O: Default,
{
    let center = Vec2 { x: 0.5, y: 0.5 };
    let aspect = width as f32 / height as f32;
    let interaction_transform = Affine2::from_cols(Vec2::X, Vec2::NEG_Y * aspect, center);
    let mouse_transform = Affine2::from_cols(
        vec2(width as f32, 0.),
        vec2(0., height as f32),
        vec2(0.0, 0.0),
    );

    let state = OverlayState {
        name: "WayVR".into(),
        want_visible: true,
        interactable: true,
        recenter: true,
        grabbable: true,
        z_order: 100,
        spawn_scale: 2.0,
        spawn_point: vec3a(0.0, -0.5, 0.0),
        interaction_transform,
        ..Default::default()
    };

    let renderer = WayVRRenderer::new(app, width, height)?;
    let context = renderer.context.clone();

    let backend = Box::new(SplitOverlayBackend {
        renderer: Box::new(renderer),
        interaction: Box::new(WayVRInteractionHandler::new(context, mouse_transform)),
    });

    Ok(OverlayData {
        state,
        backend,
        ..Default::default()
    })
}
