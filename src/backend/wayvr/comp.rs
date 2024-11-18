use smithay::backend::renderer::utils::on_commit_buffer_handler;
use smithay::input::{Seat, SeatHandler, SeatState};
use smithay::reexports::calloop::{EventLoop, LoopHandle};
use smithay::reexports::wayland_protocols::xdg::shell::server::xdg_toplevel;
use smithay::reexports::wayland_server;
use smithay::reexports::wayland_server::protocol::{wl_buffer, wl_seat, wl_surface};
use smithay::reexports::wayland_server::Resource;
use smithay::wayland::buffer::BufferHandler;
use smithay::wayland::shm::{ShmHandler, ShmState};
use smithay::wayland::xdg_foreign::{XdgForeignHandler, XdgForeignState};
use smithay::wayland::xwayland_keyboard_grab::XWaylandKeyboardGrabHandler;
use smithay::wayland::xwayland_shell::{self, XWaylandShellHandler, XWaylandShellState};
use smithay::xwayland::xwm::{Reorder, ResizeEdge, XwmId};
use smithay::xwayland::{X11Surface, X11Wm, XWaylandClientData, XwmHandler};
use smithay::{
    delegate_compositor, delegate_data_device, delegate_seat, delegate_shm, delegate_xdg_foreign,
    delegate_xdg_shell, delegate_xwayland_keyboard_grab, delegate_xwayland_shell,
};
use std::cell::RefCell;
use std::collections::HashSet;
use std::os::fd::OwnedFd;
use std::rc::Rc;

use smithay::utils::{Logical, Rectangle, Serial};
use smithay::wayland::compositor::{
    self, with_surface_tree_downward, SurfaceAttributes, TraversalAction,
};

use smithay::wayland::selection::data_device::{
    ClientDndGrabHandler, DataDeviceHandler, DataDeviceState, ServerDndGrabHandler,
};
use smithay::wayland::selection::{SelectionHandler, SelectionSource, SelectionTarget};
use smithay::wayland::shell::xdg::{
    PopupSurface, PositionerState, ToplevelSurface, XdgShellHandler, XdgShellState,
};
use wayland_server::backend::{ClientData, ClientId, DisconnectReason};
use wayland_server::protocol::wl_surface::WlSurface;
use wayland_server::Client;

use super::event_queue::SyncEventQueue;
use super::WayVRTask;

pub struct Application {
    pub compositor: compositor::CompositorState,
    pub xdg_shell: XdgShellState,
    pub seat_state: SeatState<Application>,
    pub shm: ShmState,
    pub data_device: DataDeviceState,
    pub wayvr_tasks: SyncEventQueue<WayVRTask>,
    pub redraw_requests: HashSet<wayland_server::backend::ObjectId>,
    pub xdg_foreign_state: XdgForeignState,
    pub event_loop: Rc<RefCell<EventLoop<'static, Application>>>, // sorry!
    pub xwayland_shell_state: xwayland_shell::XWaylandShellState,
    pub loop_handle: LoopHandle<'static, Self>,
    pub xwm: Option<X11Wm>,
    pub xdisplay_number: Option<u32>,
}

impl Application {
    pub fn check_redraw(&mut self, surface: &WlSurface) -> bool {
        self.redraw_requests.remove(&surface.id())
    }
}

impl compositor::CompositorHandler for Application {
    fn compositor_state(&mut self) -> &mut compositor::CompositorState {
        &mut self.compositor
    }

    fn client_compositor_state<'a>(
        &self,
        client: &'a Client,
    ) -> &'a compositor::CompositorClientState {
        if let Some(state) = client.get_data::<XWaylandClientData>() {
            return &state.compositor_state;
        }
        if let Some(state) = client.get_data::<ClientState>() {
            return &state.compositor_state;
        }
        panic!("Unknown client data type")
    }

    fn commit(&mut self, surface: &WlSurface) {
        on_commit_buffer_handler::<Self>(surface);
        self.redraw_requests.insert(surface.id());
    }
}

impl SeatHandler for Application {
    type KeyboardFocus = WlSurface;
    type PointerFocus = WlSurface;
    type TouchFocus = WlSurface;

    fn seat_state(&mut self) -> &mut SeatState<Self> {
        &mut self.seat_state
    }

    fn focus_changed(&mut self, _seat: &Seat<Self>, _focused: Option<&WlSurface>) {}
    fn cursor_image(
        &mut self,
        _seat: &Seat<Self>,
        _image: smithay::input::pointer::CursorImageStatus,
    ) {
    }
}

impl BufferHandler for Application {
    fn buffer_destroyed(&mut self, _buffer: &wl_buffer::WlBuffer) {}
}

impl ClientDndGrabHandler for Application {}

impl ServerDndGrabHandler for Application {
    fn send(&mut self, _mime_type: String, _fd: OwnedFd, _seat: Seat<Self>) {}
}

impl DataDeviceHandler for Application {
    fn data_device_state(&self) -> &DataDeviceState {
        &self.data_device
    }
}

impl SelectionHandler for Application {
    type SelectionUserData = ();
}

#[derive(Default)]
pub struct ClientState {
    compositor_state: compositor::CompositorClientState,
}

impl ClientData for ClientState {
    fn initialized(&self, client_id: ClientId) {
        log::debug!("Client ID {:?} connected", client_id);
    }

    fn disconnected(&self, client_id: ClientId, reason: DisconnectReason) {
        log::debug!(
            "Client ID {:?} disconnected. Reason: {:?}",
            client_id,
            reason
        );
    }
}

impl AsMut<compositor::CompositorState> for Application {
    fn as_mut(&mut self) -> &mut compositor::CompositorState {
        &mut self.compositor
    }
}

impl XdgShellHandler for Application {
    fn xdg_shell_state(&mut self) -> &mut XdgShellState {
        &mut self.xdg_shell
    }

    fn new_toplevel(&mut self, surface: ToplevelSurface) {
        if let Some(client) = surface.wl_surface().client() {
            self.wayvr_tasks
                .send(WayVRTask::NewToplevel(client.id(), surface.clone()));
        }
        surface.with_pending_state(|state| {
            state.states.set(xdg_toplevel::State::Activated);
        });
        surface.send_configure();
    }

    fn new_popup(&mut self, _surface: PopupSurface, _positioner: PositionerState) {
        // Handle popup creation here
    }

    fn grab(&mut self, _surface: PopupSurface, _seat: wl_seat::WlSeat, _serial: Serial) {
        // Handle popup grab here
    }

    fn reposition_request(
        &mut self,
        _surface: PopupSurface,
        _positioner: PositionerState,
        _token: u32,
    ) {
        // Handle popup reposition here
    }
}

impl ShmHandler for Application {
    fn shm_state(&self) -> &ShmState {
        &self.shm
    }
}

// see smithay: src/xwayland/xwm/mod.rs, we do bare minimum here to make it work
impl XwmHandler for Application {
    fn xwm_state(&mut self, _xwm: XwmId) -> &mut X11Wm {
        self.xwm.as_mut().unwrap()
    }

    fn new_window(&mut self, _xwm: XwmId, _window: X11Surface) {}

    fn new_override_redirect_window(&mut self, _xwm: XwmId, _window: X11Surface) {}

    fn map_window_request(&mut self, _xwm: XwmId, window: X11Surface) {
        log::debug!("map_window_request called");
        window.set_mapped(true).unwrap();
    }

    fn mapped_override_redirect_window(&mut self, _xwm: XwmId, _window: X11Surface) {
        log::debug!("mapped_override_redirect_window called");
    }

    fn unmapped_window(&mut self, _xwm: XwmId, _window: X11Surface) {
        log::debug!("unmapped_window called");
    }

    fn destroyed_window(&mut self, _xwm: XwmId, _window: X11Surface) {
        log::debug!("destroyed_window called");
    }

    fn configure_request(
        &mut self,
        _xwm: XwmId,
        window: X11Surface,
        _x: Option<i32>,
        _y: Option<i32>,
        w: Option<u32>,
        h: Option<u32>,
        _reorder: Option<Reorder>,
    ) {
        log::debug!("configure_request called");
        let mut geo = window.geometry();
        if let Some(w) = w {
            geo.size.w = w as i32;
        }
        if let Some(h) = h {
            geo.size.h = h as i32;
        }
        let _ = window.configure(geo);
    }

    fn configure_notify(
        &mut self,
        _xwm: XwmId,
        _window: X11Surface,
        _geometry: Rectangle<i32, Logical>,
        _above: Option<u32>,
    ) {
        log::debug!("configure_notify called");
    }

    fn resize_request(
        &mut self,
        _xwm: XwmId,
        _window: X11Surface,
        _button: u32,
        _edges: ResizeEdge,
    ) {
        log::debug!("resize_request called");
    }

    fn move_request(&mut self, _xwm: XwmId, _window: X11Surface, _button: u32) {
        log::debug!("move_request called");
    }
}

impl XWaylandShellHandler for Application {
    fn xwayland_shell_state(&mut self) -> &mut XWaylandShellState {
        &mut self.xwayland_shell_state
    }
}

impl XdgForeignHandler for Application {
    fn xdg_foreign_state(&mut self) -> &mut XdgForeignState {
        &mut self.xdg_foreign_state
    }
}

delegate_xdg_shell!(Application);
delegate_compositor!(Application);
delegate_shm!(Application);
delegate_seat!(Application);
delegate_data_device!(Application);
delegate_xwayland_shell!(Application);
delegate_xdg_foreign!(Application);

pub fn send_frames_surface_tree(surface: &wl_surface::WlSurface, time: u32) {
    with_surface_tree_downward(
        surface,
        (),
        |_, _, &()| TraversalAction::DoChildren(()),
        |_surf, states, &()| {
            // the surface may not have any user_data if it is a subsurface and has not
            // yet been commited
            for callback in states
                .cached_state
                .get::<SurfaceAttributes>()
                .current()
                .frame_callbacks
                .drain(..)
            {
                callback.done(time);
            }
        },
        |_, _, &()| true,
    );
}
