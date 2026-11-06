# Independent Wow / Flutter, 0.5.0

Tape now processes the main effect signal before Delay and finishing EQ. It works while Delay is OFF.
Master Mix blends an aligned, untouched dry reference with the whole processed chain. Tape OFF and
Tape ON with both depths at zero are transparent, apart from the reported fixed latency.

The plugin reports a constant nominal 6 ms latency (288 samples at 48 kHz). Dry, processed-neutral,
Master Mix 0% and host-bypassed paths all share this delay, so enabling Tape does not change host
latency or introduce a static dry/wet offset. Ableton compensates playback timing; live monitoring
still includes the physical processing latency. Latency is fixed even with all effects off.

## Movement

Independent reproducible pseudo-random sources drive sixth-order low-pass filters at a 1 kHz control
rate. Slow Wow drift (0.18 Hz) and faster Wow movement (3.7 Hz) form one control; Flutter uses 25 Hz.
Control points are cubically interpolated at audio rate and softly bounded. These produce smooth,
irregular movement rather than a pair of fixed oscillators. Both audio channels use the same read
position, preserving stereo polarity and balance. Seeds reset in prepare for repeatable rendering.

Depth changes smooth over 80 ms. Tape bypass crossfades over 20 ms while its buffer and modulation
continue running. Maximum displacement is bounded and read-head speed is limited, including at
extreme settings. The random filters warm before playback, eliminating their initial statistical ramp.
No processing-time allocations are required.

A normalized 64-tap Blackman-windowed sinc interpolator with 1024 fractional phases reads the Tape
buffer. The explicit dry-reference read is exact. High-frequency accuracy is checked against an
analytically resampled sine, including 20 kHz at 44.1/48 kHz. This first version does not claim that
pitch shifting arbitrary near-Nyquist input is alias-free.

## Reference and verification

The user's ValhallaDelay Tape/Past captures at 50% measured 4.924 cents RMS Wow and 7.094 cents RMS
Flutter. Prototype renders at 50% measured approximately 4.89 and 7.11 cents RMS on a 3150 Hz tone.
The dominant movement bands agree broadly; the actual random traces intentionally differ. Steady
3150 Hz output level remains within 0.01 dB of the neutral path. Raw reference recordings stay local.

Core checks cover fixed latency, neutral/bypassed passthrough, sample-rate consistency, independent
Tape with Delay off, global Mix endpoints/midpoint, stereo polarity, level, bounds, no allocations,
parameter/bypass stress, and high-frequency interpolation. Native checks verify reported host latency
and host-bypassed impulse timing as well as editor behavior. Musical listening in Live 11 remains the
next acceptance step; these numerical tests do not establish final sound quality.

All existing parameter IDs and defaults remain. Older nonzero Wow/Flutter settings retain their
values, but their sound changes intentionally because Tape now acts on the main signal. New instances
still start with Delay/Tape/EQ off, Wow/Flutter zero, Master Mix 100%, Output 0 dB.

Age, saturation, noise and intentional tape tone shaping are postponed.

## 0.6.0 editor and preset update

The 0.5.0 Tape algorithm is unchanged. Editor-size retention, the Input/Output drawer and the
preset/Save As workflow are implemented in 0.6.0. See PRESETS-NOTES.md for state and file behavior.
Input trim precedes both the processed chain and its dry reference; Output trim follows Master Mix.
Both are smoothed and neutral at 0 dB. They are outside the effects blend.
