// Clip view (Xaza, after Ableton's clip properties): the selected playlist clip's start, length, offset, gain and fades
// as − / + rows, plus its independence from the clips it was copied from.

use os_ui::{Font, Rect};

use super::FrameInfo;
use crate::app::{Action, State, Ui, Win};
use crate::icons::{self, Icon};
use crate::playlist::ClipProp;

const W: f32 = 330.0;
const HEAD: f32 = 28.0;
const ROW: f32 = 26.0;
pub const SIZE: (f32, f32) = (W, HEAD + 12.0 + 9.0 * ROW);

pub struct ClipViewUi {
    pub pos: (f32, f32),
}

impl Default for ClipViewUi {
    fn default() -> Self {
        ClipViewUi { pos: (340.0, 140.0) }
    }
}

pub fn rect(s: &State) -> Rect {
    Rect::new(s.clipview.pos.0, s.clipview.pos.1, SIZE.0, SIZE.1)
}

pub fn bounds(s: &State) -> Rect {
    rect(s).outset(1.0)
}

pub fn paint(ui: &mut Ui, s: &State, _f: &FrameInfo) {
    let t = ui.t;
    let win = rect(s);
    ui.c.fill_rect(win, t.s(0x16191a));
    ui.c.fill_rect(win.inset(1.0), t.s(0x202324));
    ui.hit(win, Action::Swallow, "");
    ui.hit(Rect::new(win.x + 1.0, win.y + 1.0, win.w - 2.0, HEAD - 2.0), Action::WinDrag(Win::ClipView), "");
    ui.c.text(Font::new(17), win.x + 9.0, win.y + 5.0, t.text, "Clip view");
    let close = Rect::new(win.right() - 19.0, win.y + 9.0, 10.0, 10.0);
    let hov = ui.hit(close.outset(3.0), Action::CloseWin(Win::ClipView), "Close");
    icons::draw(ui.c, Icon::Close, close.outset(1.0), if hov { t.tx(0xffffff) } else { t.glyph });
    let rows = crate::playlist::clip_rows(s);
    if rows.is_empty() {
        ui.c.text(Font::new(15), win.x + 12.0, win.y + HEAD + 12.0, t.s(0x8a9296), "Select a clip in the Playlist");
        return;
    }
    for (k, row) in rows.iter().enumerate() {
        let y = win.y + HEAD + 8.0 + k as f32 * ROW;
        ui.c.text(Font::new(15), win.x + 12.0, y + 3.0, t.s(0x8a9296), row.label);
        ui.c.text_clip(Font::new(15), win.x + 108.0, y + 3.0, 130.0, t.text, &row.value);
        for (n, (glyph, act)) in [("-", row.minus), ("+", row.plus)].into_iter().enumerate() {
            let Some(p) = act else { continue };
            let b = Rect::new(win.right() - 74.0 + n as f32 * 34.0, y, 30.0, ROW - 4.0);
            let hov = ui.hit(b, Action::ClipProp(p), row.label);
            ui.c.fill_rrect(b, 4.0, if hov { t.s(0x3a3f42) } else { t.s(0x2e3133) });
            ui.c.text(Font::new(17), b.x + 10.0, b.y + 1.0, t.text, glyph);
        }
    }
}
