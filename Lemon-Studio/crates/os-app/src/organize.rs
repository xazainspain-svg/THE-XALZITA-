//! Organize (Xaza, after Ableton's tidy sets): gives everything a name and a colour that agree. Channels without a
//! colour get one from a palette, the mixer tracks channels play on take their channel's name and colour, the playlist
//! tracks take those of the clips on them, and patterns with notes get a colour. Names and colours already chosen stay.

use os_model::{ClipSource, Rgb};

use crate::app::State;

/// Ableton's clip colours, in the order new ones are handed out.
const PALETTE: [(u8, u8, u8); 14] = [
    (0xff, 0x94, 0xa6),
    (0xff, 0xa5, 0x29),
    (0xcc, 0x9f, 0x99),
    (0xf7, 0xf4, 0x7c),
    (0xbf, 0xfb, 0x00),
    (0x1a, 0xff, 0x2f),
    (0x25, 0xff, 0xa8),
    (0x5c, 0xff, 0xe8),
    (0x8b, 0xc5, 0xff),
    (0x54, 0x80, 0xe4),
    (0x92, 0x7c, 0xe4),
    (0xd8, 0x6c, 0xe4),
    (0xe5, 0x53, 0xa0),
    (0xff, 0xff, 0xff),
];

fn palette(i: usize) -> Rgb {
    let (r, g, b) = PALETTE[i % PALETTE.len()];
    Rgb { r, g, b }
}

/// Names and colours as they are, for undo.
#[derive(Debug, Clone, PartialEq)]
pub struct Labels {
    channels: Vec<(u16, Option<String>, Option<Rgb>)>,
    mixer: Vec<(u16, Option<String>, Option<Rgb>)>,
    tracks: Vec<(Option<String>, Option<Rgb>)>,
    patterns: Vec<(u16, Option<String>, Option<Rgb>)>,
}

impl Labels {
    pub fn now(s: &State) -> Labels {
        Labels {
            channels: s.project.channels.iter().map(|c| (c.index, c.name.clone(), c.color)).collect(),
            mixer: s.project.mixer.iter().map(|m| (m.index, m.name.clone(), m.color)).collect(),
            tracks: s.project.arrangement().map_or_else(Vec::new, |a| a.tracks.iter().map(|t| (t.name.clone(), t.color)).collect()),
            patterns: s.project.patterns.iter().map(|p| (p.number, p.name.clone(), p.color)).collect(),
        }
    }

    pub fn put(&self, s: &mut State) {
        for (i, n, c) in &self.channels {
            if let Some(x) = s.project.channels.iter_mut().find(|x| x.index == *i) {
                x.name = n.clone();
                x.color = *c;
            }
        }
        for (i, n, c) in &self.mixer {
            if let Some(x) = s.project.mixer.iter_mut().find(|x| x.index == *i) {
                x.name = n.clone();
                x.color = *c;
            }
        }
        for (t, (n, c)) in crate::playlist::arr_mut(s).tracks.iter_mut().zip(&self.tracks) {
            t.name = n.clone();
            t.color = *c;
        }
        for (i, n, c) in &self.patterns {
            if let Some(x) = s.project.patterns.iter_mut().find(|x| x.number == *i) {
                x.name = n.clone();
                x.color = *c;
            }
        }
        s.piano.dirty = true;
    }
}

/// Fills in names and colours; returns what it changed, for the message.
pub fn organize(s: &mut State) -> String {
    let mut n = 0;
    // Channels: a colour each.
    let mut next = 0;
    for c in &mut s.project.channels {
        if c.color.is_none() {
            c.color = Some(palette(next));
            n += 1;
        }
        next += 1;
    }
    // Patterns with notes: a colour each, after the channels' run.
    for p in s.project.patterns.iter_mut().filter(|p| !p.notes.is_empty() && p.color.is_none()) {
        p.color = Some(palette(next));
        next += 1;
        n += 1;
    }
    // Mixer tracks take the name and colour of the first channel that plays on them.
    let chans: Vec<(u16, Option<String>, Option<Rgb>)> = s.project.channels.iter().map(|c| (c.mixer_track, c.name.clone(), c.color)).collect();
    for m in s.project.mixer.iter_mut().filter(|m| m.index != 0) {
        if let Some((_, name, color)) = chans.iter().find(|(t, ..)| *t == m.index) {
            if m.name.is_none() && name.is_some() {
                m.name = name.clone();
                n += 1;
            }
            if m.color.is_none() && color.is_some() {
                m.color = *color;
                n += 1;
            }
        }
    }
    // Playlist track `n` takes the name and colour of mixer insert `n + 1` (they are linked, `crate::link`).
    let mix: Vec<(u16, Option<String>, Option<Rgb>)> = s.project.mixer.iter().filter(|m| m.index > 0).map(|m| (m.index - 1, m.name.clone(), m.color)).collect();
    for (t, name, color) in mix {
        if let Some(tr) = crate::playlist::arr_mut(s).tracks.get_mut(usize::from(t)) {
            if tr.name.is_none() && name.is_some() {
                tr.name = name;
                n += 1;
            }
            if tr.color.is_none() && color.is_some() {
                tr.color = color;
                n += 1;
            }
        }
    }
    // Playlist tracks left over take the name and colour of the first clip on them.
    let clips: Vec<(u16, ClipSource)> = crate::playlist::clips(s).iter().map(|c| (c.track, c.source)).collect();
    let mut tags: Vec<(u16, Option<String>, Option<Rgb>)> = Vec::new();
    for (track, src) in clips {
        if tags.iter().any(|t| t.0 == track) {
            continue;
        }
        let (name, color) = match src {
            ClipSource::Pattern(p) => s.project.patterns.iter().find(|x| x.number == p).map_or((None, None), |x| (Some(x.name.clone().unwrap_or_else(|| format!("Pattern {p}"))), x.color)),
            ClipSource::Channel(c) => s.project.channel(c).map_or((None, None), |x| (x.name.clone(), x.color)),
        };
        tags.push((track, name, color));
    }
    for (track, name, color) in tags {
        if let Some(t) = crate::playlist::arr_mut(s).tracks.get_mut(usize::from(track)) {
            if t.name.is_none() && name.is_some() {
                t.name = name;
                n += 1;
            }
            if t.color.is_none() && color.is_some() {
                t.color = color;
                n += 1;
            }
        }
    }
    s.piano.dirty = true;
    let mut link = std::mem::take(&mut s.link);
    link.resync(s);
    s.link = link;
    if n == 0 { "Already organized: every name and colour is set".into() } else { format!("Organized: {n} names and colours filled in") }
}
