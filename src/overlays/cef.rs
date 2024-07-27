use core::str;
use std::slice;
#[allow(dead_code)]
use std::sync::Arc;

use glam::{vec2, vec3a, Affine2, Vec2};
use libc::strlen;
use vulkano::{
    buffer::{Buffer, BufferCreateInfo, BufferUsage, Subbuffer},
    command_buffer::{CommandBufferUsage, CopyBufferToImageInfo},
    format::Format,
    image::{view::ImageView, Image},
    memory::allocator::{AllocationCreateInfo, MemoryTypeFilter},
    DeviceSize,
};

use crate::{
    backend::{
        input::{self, InteractionHandler},
        overlay::{OverlayData, OverlayRenderer, OverlayState, SplitOverlayBackend},
    },
    graphics::WlxGraphics,
    state::AppState,
};

#[link(name = "wlxcefwrapper")]
extern "C" {
    fn wlxcef_get_error() -> *const libc::c_char;
    fn wlxcef_init() -> libc::c_int;
    fn wlxcef_free() -> libc::c_int;
    fn wlxcef_create_tab() -> libc::c_int;
    fn wlxcef_tick_message_loop() -> libc::c_void;
    fn wlxcef_free_tab(tab_id: libc::c_int) -> libc::c_int;
    fn wlxcef_get_viewport_width(tab_id: libc::c_int) -> libc::c_int;
    fn wlxcef_get_viewport_height(tab_id: libc::c_int) -> libc::c_int;
    fn wlxcef_get_viewport_data_rgba(tab_id: libc::c_int) -> *const libc::c_void;
    fn wlxcef_mouse_move(tab_id: libc::c_int, x: libc::c_int, y: libc::c_int) -> libc::c_int;
    fn wlxcef_mouse_set_state(
        tab_id: libc::c_int,
        mouse_index: libc::c_int,
        down: libc::c_int,
    ) -> libc::c_void;
    fn wlxcef_mouse_scroll(tab_id: libc::c_int, delta: libc::c_float);
}

fn cstring_to_rust_string(cstr: *const libc::c_char) -> &'static str {
    let s = unsafe {
        str::from_utf8_unchecked(slice::from_raw_parts(cstr as *const u8, strlen(cstr) + 1))
    };
    s
}

fn get_error_msg() -> &'static str {
    unsafe { cstring_to_rust_string(wlxcef_get_error()) }
}

struct CEFContext {
    tab_id: i32,
}

impl CEFContext {
    pub fn new() -> anyhow::Result<Self> {
        unsafe {
            let res = wlxcef_init();
            if res < 0 {
                panic!("wlxcef_init failed: {}", get_error_msg());
            }

            let tab_id = wlxcef_create_tab();
            if tab_id < 0 {
                panic!("wlxcef_create_tab failed: {}", get_error_msg());
            }

            Ok(Self { tab_id })
        }
    }
}

impl Drop for CEFContext {
    fn drop(&mut self) {
        unsafe {
            let res = wlxcef_free();
            if res < 0 {
                panic!("wlxcef_free failed: {}", get_error_msg());
            }
        }
    }
}

const IMAGE_WIDTH: u32 = 1280;
const IMAGE_HEIGHT: u32 = 720;

fn load_image(
    graphics: Arc<WlxGraphics>,
    width: u32,
    height: u32,
    data: &[u8],
) -> anyhow::Result<Arc<Image>> {
    let mut upload_buffer = graphics.create_command_buffer(CommandBufferUsage::OneTimeSubmit)?;

    let image = upload_buffer.texture2d_raw(width, height, Format::R8G8B8A8_UNORM, data)?;
    upload_buffer.build_and_execute_now()?;
    Ok(image)
}

fn update_image(image: &Arc<Image>, graphics: Arc<WlxGraphics>, data: &[u8]) -> anyhow::Result<()> {
    let mut command_buffer = graphics.create_command_buffer(CommandBufferUsage::OneTimeSubmit)?;

    let buffer: Subbuffer<[u8]> = Buffer::new_slice(
        graphics.memory_allocator.clone(),
        BufferCreateInfo {
            usage: BufferUsage::TRANSFER_SRC,
            ..Default::default()
        },
        AllocationCreateInfo {
            memory_type_filter: MemoryTypeFilter::PREFER_HOST
                | MemoryTypeFilter::HOST_SEQUENTIAL_WRITE,
            ..Default::default()
        },
        data.len() as DeviceSize,
    )?;

    buffer.write()?.copy_from_slice(data);

    command_buffer
        .command_buffer
        .copy_buffer_to_image(CopyBufferToImageInfo::buffer_image(buffer, image.clone()))?;

    command_buffer.build_and_execute_now()?;
    Ok(())
}

pub struct CEFInteractionHandler {
    context: Arc<CEFContext>,
    mouse_transform: Affine2,
}

impl CEFInteractionHandler {
    pub fn new(context: Arc<CEFContext>, mouse_transform: Affine2) -> Self {
        Self {
            context,
            mouse_transform,
        }
    }
}

pub struct CEFRenderer {
    image: Arc<Image>,
    view: Arc<ImageView>,
    context: Arc<CEFContext>,
    graphics: Arc<WlxGraphics>,
}

impl InteractionHandler for CEFInteractionHandler {
    fn on_hover(&mut self, _app: &mut AppState, hit: &input::PointerHit) -> Option<input::Haptics> {
        let tab_id = self.context.tab_id;
        let pos = self.mouse_transform.transform_point2(hit.uv);
        unsafe {
            wlxcef_mouse_move(tab_id, pos.x as i32, pos.y as i32);
        }
        None
    }

    fn on_left(&mut self, _app: &mut AppState, _pointer: usize) {}

    fn on_pointer(&mut self, _app: &mut AppState, hit: &input::PointerHit, pressed: bool) {
        let tab_id = self.context.tab_id;
        unsafe {
            let index = match hit.mode {
                input::PointerMode::Left => 0,
                input::PointerMode::Right => 1,
                input::PointerMode::Middle => 2,
                input::PointerMode::Special => 0,
            };

            wlxcef_mouse_set_state(tab_id, index, pressed as i32);
        }
    }

    fn on_scroll(&mut self, _app: &mut AppState, _hit: &input::PointerHit, delta: f32) {
        let tab_id = self.context.tab_id;
        unsafe {
            wlxcef_mouse_scroll(tab_id, delta);
        }
    }
}

impl CEFRenderer {
    pub fn new(app: &mut AppState) -> anyhow::Result<Self> {
        let mut data: Vec<u8> = vec![100; (IMAGE_WIDTH * IMAGE_HEIGHT * 4) as usize];

        // Test pattern
        for i in 0..data.len() - 1 {
            data[i] = (i % 256) as u8;
        }

        let image = load_image(
            app.graphics.clone(),
            IMAGE_WIDTH,
            IMAGE_HEIGHT,
            data.as_slice(),
        )?;

        let view = ImageView::new_default(image.clone()).unwrap();

        Ok(Self {
            image,
            view,
            context: Arc::new(CEFContext::new()?),
            graphics: app.graphics.clone(),
        })
    }
}

impl OverlayRenderer for CEFRenderer {
    fn init(&mut self, _app: &mut AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn pause(&mut self, _app: &mut AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn resume(&mut self, _app: &mut AppState) -> anyhow::Result<()> {
        Ok(())
    }

    fn render(&mut self, _app: &mut AppState) -> anyhow::Result<()> {
        unsafe {
            wlxcef_tick_message_loop();

            let tab_id = self.context.tab_id;
            let viewport_width = wlxcef_get_viewport_width(tab_id);
            let viewport_height = wlxcef_get_viewport_height(tab_id);
            let data = wlxcef_get_viewport_data_rgba(tab_id);
            if !data.is_null() {
                let data_slice = std::slice::from_raw_parts(
                    data as *const u8,
                    (viewport_width * viewport_height * 4) as usize,
                );

                update_image(&self.image, self.graphics.clone(), data_slice).unwrap();
            } else {
                log::info!("CEF render data is null: {}", get_error_msg());
            }
        }
        Ok(())
    }

    fn view(&mut self) -> Option<Arc<ImageView>> {
        Some(self.view.clone())
    }
}

pub fn create_cef<O>(app: &mut AppState) -> anyhow::Result<OverlayData<O>>
where
    O: Default,
{
    let center = Vec2 { x: 0.5, y: 0.5 };
    let interaction_transform = Affine2::from_cols(Vec2::X, Vec2::NEG_Y * (1280.0 / 720.0), center);
    let mouse_transform = Affine2::from_cols(vec2(1280.0, 0.), vec2(0., 720.0), vec2(0.0, 0.0));

    let state = OverlayState {
        name: "CEF Renderer".into(),
        want_visible: true,
        interactable: true,
        recenter: true,
        grabbable: true,
        z_order: 100,
        spawn_scale: 1.0,
        spawn_point: vec3a(0.0, -0.5, 0.0),
        interaction_transform,
        ..Default::default()
    };

    let renderer = CEFRenderer::new(app)?;
    let context = renderer.context.clone();

    let backend = Box::new(SplitOverlayBackend {
        renderer: Box::new(renderer),
        interaction: Box::new(CEFInteractionHandler::new(context, mouse_transform)),
    });

    Ok(OverlayData {
        state,
        backend,
        ..Default::default()
    })
}
