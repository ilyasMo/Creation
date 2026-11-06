# Native delay checkpoint

This is the first native effect, independently developed from the browser delay. Movement, Reverb,
Texture, finishing EQ, and custom interface remain future native work. The agreed final routing stays
Texture → Movement → Delay → Reverb → EQ, with Master Mix around the complete processed path.

Two stereo ring buffers use four-point interpolation. Delay-time changes crossfade read taps over 40 ms;
rapid updates retain the latest requested target. Controls smooth over about 20 ms. The feedback path
contains a 50 Hz high-pass, two low-pass damping stages and a controllable soft saturator, so later repeats
lose high frequencies and can acquire saturation. Ping-pong blends independent and cross-channel feedback.
Mono hosts keep a mono delay. The original input is retained with a fixed internal echo return; there is
no user-facing effect gain control. This is insert-style processing, not a wet-only send effect.

Tempo comes from the host, falling back to the last known value or 120 BPM. Synced notes span whole
through 64th notes with straight, dotted and triplet styles. The maximum buffer is 24 seconds.

All processing is OFF on a new instance; Master Mix is 100%, output gain 0 dB. Existing outputGain
parameter ID is preserved. Missing parameters in older gain-only sessions are restored to defaults.

Portable tests cover mono/stereo passthrough, zero Mix, 44.1/48/96 kHz, impulse timing, note calculations,
feedback darkening, ping-pong and high-feedback retiming stress. They do not prove subjective sound quality,
real host tempo/automation accuracy, alias-free saturation, or perfect loudness matching. Live 11 listening,
sync, automation and saved-state checks are the next user-side validation.
