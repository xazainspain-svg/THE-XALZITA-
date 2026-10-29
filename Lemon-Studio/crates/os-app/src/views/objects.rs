// Object mixer (Xaza): a radar around the listener. Click places the selected mixer track in space (headphones); a
// click near a placed track selects it instead. Not Dolby Atmos (licensed): a spherical-head model (os_dsp::spatial).

use os_ui::{Font, Rect};

use super::FrameInfo;
use crate::app::{Action, State, Ui, Win};
use crate::icons::{self, Icon};

const R: f32 = 130.0;
const HEAD: f32 = 28.0;
pub const SIZE: (f32, f32) = (2.0 * R + 40.0, HEAD + 2.0 * R + 70.0);

pub struct ObjectsUi {
    pub pos: (f32, f32),
}

impl Default for ObjectsUi {
    fn default() -> Self {
        ObjectsUi { pos: (300.0, 120.0) }
    }
}

pub fn rect(s: &State) -> Rect {
    Rect::new(s.objects.pos.0, s.objects.pos.1, SIZE.0, SIZE.1)
}

pub fn bounds(s: &State) -> Rect {
    rect(s).outset(1.0)
}

/// The radar's disc on screen.
pub fn radar(s: &State) -> (f32, f32) {
    let w = rect(s);
    (w.x + w.w / 2.0, w.y + HEAD + 12.0 + R)
}

/// Azimuth (degrees, 0 = ahead = up, 90 = right) and distance (0..=1) of a point on the radar.
pub fn place_at(s: &State, x: f32, y: f32) -> (f32, f32) {
    let (cx, cy) = radar(s);
    let (dx, dy) = (x - cx, y - cy);
    (dx.atan2(-dy).to_degrees(), (dx.hypot(dy) / R).min(1.0))
}

/// The dot of a placed track on screen.
pub fn dot(s: &State, az: f32, dist: f32) -> (f32, f32) {
    let (cx, cy) = radar(s);
    let a = az.to_radians();
    (cx + a.sin() * dist * R, cy - a.cos() * dist * R)
}

pub fn paint(ui: &mut Ui, s: &State, _f: &FrameInfo) {
    let t = ui.t;
    let win = rect(s);
    ui.c.fill_rect(win, t.s(0x16191a));
    ui.c.fill_rect(win.inset(1.0), t.s(0x202324));
    ui.hit(win, Action::Swallow, "");
    ui.hit(Rect::new(win.x + 1.0, win.y + 1.0, win.w - 2.0, HEAD - 2.0), Action::WinDrag(Win::Objects), "");
    ui.c.text(Font::new(17), win.x + 9.0, win.y + 5.0, t.text, "Object mixer (headphones)");
    let close = Rect::new(win.right() - 19.0, win.y + 9.0, 10.0, 10.0);
    let hov = ui.hit(close.outset(3.0), Action::CloseWin(Win::Objects), "Close");
    icons::draw(ui.c, Icon::Close, close.outset(1.0), if hov { t.tx(0xffffff) } else { t.glyph });
    let (cx, cy) = radar(s);
    let disc = Rect::new(cx - R, cy - R, 2.0 * R, 2.0 * R);
    ui.hit(disc, Action::ObjPlace(false), "Click: place the selected mixer track; click a dot: select its track");
    ui.c.fill_circle(cx, cy, R, t.s(0x2a2d2f));
    ui.c.stroke_circle(cx, cy, R, 1.5, t.s(0x4a4f52));
    ui.c.stroke_circle(cx, cy, R * 0.5, 1.0, t.s(0x34383a));
    ui.c.fill_rect(Rect::new(cx - 0.5, cy - R, 1.0, 2.0 * R), t.s(0x34383a));
    ui.c.fill_rect(Rect::new(cx - R, cy - 0.5, 2.0 * R, 1.0), t.s(0x34383a));
    ui.c.text(Font::new(13), cx - 6.0, cy - R + 2.0, t.s(0x8a9296), "F");
    ui.c.text(Font::new(13), cx - 5.0, cy + R - 14.0, t.s(0x8a9296), "B");
    ui.c.fill_circle(cx, cy, 6.0, t.s(0x8a9296));
    for m in s.project.mixer.iter().filter(|m| m.index != 0) {
        let Some((az, dist)) = m.spatial else { continue };
        let (x, y) = dot(s, az, dist);
        let sel = m.index == s.selected_mixer;
        ui.c.fill_circle(x, y, 9.0, if sel { t.h(0xf09a28) } else { t.s(0x6d8cff) });
        ui.c.text(Font::new(13), x - 4.0 * (m.index.to_string().len() as f32), y - 7.0, t.tx(0x111111), &m.index.to_string());
    }
    let n = s.selected_mixer;
    let state = match s.project.mixer.iter().find(|m| m.index == n).and_then(|m| m.spatial) {
        Some((az, d)) => format!("Insert {n}: {az:.0} deg, distance {:.0}%", d * 100.0),
        None => format!("Insert {n}: not placed"),
    };
    ui.c.text(Font::new(15), win.x + 10.0, win.bottom() - 52.0, t.text, &state);
    let off = Rect::new(win.x + 10.0, win.bottom() - 28.0, 120.0, 20.0);
    let hov = ui.hit(off, Action::ObjPlace(true), "Take the selected track out of the object mixer");
    ui.c.fill_rrect(off, 4.0, if hov { t.s(0x3a3f42) } else { t.s(0x2e3133) });
    ui.c.text(Font::new(15), off.x + 8.0, off.y + 2.0, t.text, "Remove from space");
}
