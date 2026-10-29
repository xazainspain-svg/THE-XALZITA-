//! Object mixer (Xaza): places a mixer track in the space around the listener over headphones. The track is summed to
//! mono and rendered binaurally with the cues that need no measured data: the interaural time difference (the far
//! ear hears it later), the level difference and head shadow (the far ear is quieter and duller), a duller sound
//! behind the head and, with distance, a quieter and duller one. It is not Dolby Atmos (licensed); the HRTF is a
//! simple spherical-head model.

/// Head radius in metres and the speed of sound in m/s (Woodworth's interaural delay).
const HEAD_R: f32 = 0.0875;
const C: f32 = 343.0;
const BUF: usize = 128;

/// A position: `azimuth` in degrees (0 = ahead, 90 = right, 180 = behind), `distance` 0 (at the head) ..= 1 (far).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Position {
    pub azimuth: f32,
    pub distance: f32,
}

#[derive(Debug, Clone)]
pub struct Spatial {
    rate: f32,
    pos: Position,
    line: [f32; BUF],
    at: usize,
    /// One-pole low-pass states for the ears and the common (distance / behind) filter.
    lp: [f32; 3],
}

fn pole(fc: f32, rate: f32) -> f32 {
    1.0 - (-2.0 * std::f32::consts::PI * fc / rate).exp()
}

impl Spatial {
    pub fn new(rate: u32, pos: Position) -> Spatial {
        Spatial { rate: rate as f32, pos, line: [0.0; BUF], at: 0, lp: [0.0; 3] }
    }

    pub fn set(&mut self, pos: Position) {
        self.pos = pos;
    }

    pub fn position(&self) -> Position {
        self.pos
    }

    /// Renders interleaved stereo frames in place: the input is summed to mono and placed.
    pub fn process(&mut self, buf: &mut [f32]) {
        let az = self.pos.azimuth.to_radians();
        let side = az.sin(); // -1 left .. 1 right
        let front = az.cos(); // 1 ahead .. -1 behind
        let dist = self.pos.distance.clamp(0.0, 1.0);
        // Woodworth: the far ear's extra delay, in samples.
        let itd = (HEAD_R / C) * (side.abs().asin() + side.abs()) * self.rate;
        let itd = itd.clamp(0.0, (BUF - 2) as f32);
        // The near ear gets a little louder, the far ear quieter and duller (head shadow).
        let g_near = 1.0 + 0.25 * side.abs();
        let g_far = 1.0 - 0.55 * side.abs();
        let a_far = pole(20000.0 - 17000.0 * side.abs(), self.rate);
        // Behind the head and far away dull the whole sound; distance also lowers it.
        let common_fc = 20000.0 - 9000.0 * (1.0 - front).max(0.0) * 0.5 - 14000.0 * dist;
        let a_common = pole(common_fc.max(1200.0), self.rate);
        let dist_gain = 1.0 / (1.0 + 2.0 * dist);
        let (far_is_left, _) = (side > 0.0, 0);
        for f in buf.chunks_exact_mut(2) {
            let m = 0.5 * (f[0] + f[1]);
            self.lp[2] += a_common * (m - self.lp[2]);
            let x = self.lp[2] * dist_gain;
            self.line[self.at] = x;
            // The far ear reads the delayed signal (linear interpolation).
            let pos = self.at as f32 + BUF as f32 - itd;
            let i0 = pos.floor();
            let frac = pos - i0;
            let (i0, i1) = (i0 as usize % BUF, (i0 as usize + 1) % BUF);
            let delayed = self.line[i0] * (1.0 - frac) + self.line[i1] * frac;
            self.at = (self.at + 1) % BUF;
            self.lp[0] += a_far * (delayed - self.lp[0]);
            let near = x * g_near;
            let far = self.lp[0] * g_far;
            if far_is_left {
                f[0] = far;
                f[1] = near;
            } else {
                f[0] = near;
                f[1] = far;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(az: f32, dist: f32) -> (f32, f32, Vec<f32>) {
        let mut s = Spatial::new(48000, Position { azimuth: az, distance: dist });
        let mut buf: Vec<f32> = (0..4800).flat_map(|i| { let v = (i as f32 * 0.05).sin() * 0.5; [v, v] }).collect();
        s.process(&mut buf);
        let (mut l, mut r) = (0.0f32, 0.0f32);
        for f in buf[2400..].chunks_exact(2) {
            l += f[0] * f[0];
            r += f[1] * f[1];
        }
        (l, r, buf)
    }

    #[test]
    fn right_is_louder_on_the_right_and_left_on_the_left() {
        let (l, r, _) = run(90.0, 0.0);
        assert!(r > 2.0 * l, "right: {l} {r}");
        let (l, r, _) = run(-90.0, 0.0);
        assert!(l > 2.0 * r, "left: {l} {r}");
    }

    #[test]
    fn ahead_is_balanced_and_distance_is_quieter() {
        let (l, r, _) = run(0.0, 0.0);
        assert!((l - r).abs() < 1e-3 * l.max(1.0), "ahead: {l} {r}");
        let (l2, r2, _) = run(0.0, 1.0);
        assert!(l2 + r2 < 0.5 * (l + r), "far is quieter");
    }

    #[test]
    fn stays_finite() {
        for az in [-180.0, -45.0, 0.0, 90.0, 180.0] {
            let (_, _, buf) = run(az, 0.7);
            assert!(buf.iter().all(|v| v.is_finite() && v.abs() < 2.0));
        }
    }
}
