// Session view (Xaza, after Ableton Live's): a window of launch pads, one per pattern. A click selects the pattern and
// plays it in PAT mode; clicking the playing one stops it.

use os_ui::{Font, Rect};

use super::FrameInfo;
use crate::app::{Action, State, Ui, Win};
use crate::icons::{self, Icon};

pub const COLS: usize = 8;
pub const ROWS: usize = 4;
const PAD_W: f32 = 70.0;
const PAD_H: f32 = 46.0;
const GAP: f32 = 4.0;
const HEAD: f32 = 28.0;
pub const SIZE: (f32, f32) = (12.0 + COLS as f32 * (PAD_W + GAP), HEAD + 10.0 + ROWS as f32 * (PAD_H + GAP));

/// Window position, set once when the state is built and moved by dragging its title.
pub struct SessionUi {
    pub pos: (f32, f32),
}

impl Default for SessionUi {
    fn default() -> Self {
        SessionUi { pos: (260.0, 140.0) }
    }
}

pub fn rect(s: &State) -> Rect {
    Rect::new(s.session.pos.0, s.session.pos.1, SIZE.0, SIZE.1)
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
    ui.hit(Rect::new(win.x + 1.0, win.y + 1.0, win.w - 2.0, HEAD - 2.0), Action::WinDrag(Win::Session), "");
    ui.c.text(Font::new(17), win.x + 9.0, win.y + 5.0, t.text, "Session - click a pad to launch its pattern");
    let close = Rect::new(win.right() - 19.0, win.y + 9.0, 10.0, 10.0);
    let hov = ui.hit(close.outset(3.0), Action::CloseWin(Win::Session), "Close");
    icons::draw(ui.c, Icon::Close, close.outset(1.0), if hov { t.tx(0xffffff) } else { t.glyph });
    let live = s.pattern;
    for k in 0..COLS * ROWS {
        let (c, r) = (k % COLS, k / COLS);
        let pad = Rect::new(win.x + 8.0 + c as f32 * (PAD_W + GAP), win.y + HEAD + 6.0 + r as f32 * (PAD_H + GAP), PAD_W, PAD_H);
        let n = (k + 1) as u16;
        let pat = s.project.patterns.iter().find(|p| p.number == n);
        let used = pat.is_some_and(|p| !p.notes.is_empty());
        let hov = ui.hit(pad, Action::Session(n), format!("Launch pattern {n}"));
        let base = pat.and_then(|p| p.color).map_or(t.s(0x3a3f42), |c| os_ui::Color::rgb((u32::from(c.r) << 16) | (u32::from(c.g) << 8) | u32::from(c.b)));
        let face = if used { base } else { t.s(0x2a2d2f) };
        ui.c.fill_rrect(pad, 4.0, if hov { face.shade(0.15) } else { face });
        if n == live {
            ui.c.stroke_rrect(pad, 4.0, 2.0, t.h(0xf09a28));
        }
        let name = pat.and_then(|p| p.name.clone()).unwrap_or_else(|| format!("Pattern {n}"));
        ui.c.text_clip(Font::new(15), pad.x + 5.0, pad.y + 4.0, pad.w - 10.0, if used { t.tx(0x111111) } else { t.s(0x6d7275) }, &name);
    }
}
