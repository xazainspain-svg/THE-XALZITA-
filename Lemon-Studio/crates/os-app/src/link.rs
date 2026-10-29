//! Linked names and colours (Xaza): playlist track `n`, mixer insert `n + 1` and the channels that play on that insert
//! share one name and one colour. Change any of them, wherever it is done, and the others follow at once.

use std::collections::HashMap;

use os_model::Rgb;

use crate::app::State;

type Label = (Option<String>, Option<Rgb>);

/// What each member last agreed on, to tell which one the user changed.
#[derive(Default)]
pub struct Linked {
    prev: HashMap<(u8, u16), Label>,
}

const CHANNEL: u8 = 0;
const MIXER: u8 = 1;
const TRACK: u8 = 2;

fn read(s: &State, key: (u8, u16)) -> Option<Label> {
    match key.0 {
        CHANNEL => s.project.channels.iter().find(|c| c.index == key.1).map(|c| (c.name.clone(), c.color)),
        MIXER => s.project.mixer.iter().find(|m| m.index == key.1).map(|m| (m.name.clone(), m.color)),
        _ => s.project.arrangement()?.tracks.get(usize::from(key.1)).map(|t| (t.name.clone(), t.color)),
    }
}

fn write(s: &mut State, key: (u8, u16), l: &Label) {
    match key.0 {
        CHANNEL => {
            if let Some(c) = s.project.channels.iter_mut().find(|c| c.index == key.1) {
                (c.name, c.color) = l.clone();
            }
        }
        MIXER => {
            if let Some(m) = s.project.mixer.iter_mut().find(|m| m.index == key.1) {
                (m.name, m.color) = l.clone();
            }
        }
        _ => {
            if let Some(t) = s.project.arrangement_mut().and_then(|a| a.tracks.get_mut(usize::from(key.1))) {
                (t.name, t.color) = l.clone();
            }
        }
    }
}

/// The members of insert `m`'s group: its channels, the insert, and playlist track `m - 1`.
fn members(s: &State, m: u16) -> Vec<(u8, u16)> {
    let mut v: Vec<(u8, u16)> = s.project.channels.iter().filter(|c| c.mixer_track == m).map(|c| (CHANNEL, c.index)).collect();
    v.push((MIXER, m));
    v.push((TRACK, m - 1));
    v.retain(|&k| read(s, k).is_some());
    v
}

impl Linked {
    /// Carries a change in any member of a group to the others; returns whether anything was written. A member seen
    /// for the first time only has its labels noted.
    pub fn sync(&mut self, s: &mut State) -> bool {
        let inserts: Vec<u16> = s.project.mixer.iter().map(|m| m.index).filter(|&i| i > 0).collect();
        let mut wrote = false;
        for m in inserts {
            let group = members(s, m);
            let changed = group.iter().copied().find(|k| match (self.prev.get(k), read(s, *k)) {
                (Some(p), Some(c)) => *p != c,
                _ => false,
            });
            if let Some(w) = changed {
                let label = read(s, w).expect("member exists");
                for &k in &group {
                    if k != w && read(s, k).as_ref() != Some(&label) {
                        write(s, k, &label);
                        wrote = true;
                    }
                }
                for &k in &group {
                    self.prev.insert(k, label.clone());
                }
            } else {
                for &k in &group {
                    if !self.prev.contains_key(&k) {
                        if let Some(c) = read(s, k) {
                            self.prev.insert(k, c);
                        }
                    }
                }
            }
        }
        if wrote {
            s.piano.dirty = true;
        }
        wrote
    }

    /// Notes every member as it is now, without carrying anything (after a change that set them all itself).
    pub fn resync(&mut self, s: &State) {
        self.prev.clear();
        for m in s.project.mixer.iter().map(|m| m.index).filter(|&i| i > 0) {
            for k in members(s, m) {
                if let Some(c) = read(s, k) {
                    self.prev.insert(k, c);
                }
            }
        }
    }
}
