# Native delay checkpoint

This is the first native effect, independently developed from the browser delay. Movement, Reverb,
Texture, finishing EQ, and custom interface remain future native work. The agreed final routing stays
Texture → Movement → Delay → Reverb → EQ, with Master Mix around the complete processed path.

Two stereo ring buffers use four-point interpolation. Delay-time changes crossfade read taps over 40 ms;
rapid updates retain the latest requested target. Controls smooth over about 20 ms. The feedback path
contains a 30 Hz high-pass, a TPT Butterworth low-pass and an antiderivative-antialiased soft saturator, so later repeats
lose high frequencies and can acquire saturation. Ping-pong blends independent and cross-channel feedback.
Mono hosts keep a mono delay. The original input is retained with a fixed internal echo return; there is
no user-facing effect gain control. This is insert-style processing, not a wet-only send effect.

Tempo comes from the host, falling back to the last known value or 120 BPM. Synced notes span whole
through 64th notes with straight, dotted and triplet styles. The maximum buffer is 24 seconds.

All processing is OFF on a new instance; Master Mix is 100%, output gain 0 dB. Existing outputGain
parameter ID is preserved. Missing parameters in older gain-only sessions are restored to defaults.

Portable tests cover mono/stereo passthrough, zero Mix, 44.1/48/96/192 kHz, impulse timing, note calculations,
feedback darkening, ping-pong and high-feedback retiming stress. They do not prove subjective sound quality,
real host tempo/automation accuracy, alias-free saturation, or perfect loudness matching. Live 11 listening,
sync, automation and saved-state checks are the next user-side validation.

## Refinement 0.2.1

The feedback tone cutoff now uses a topology-preserving Butterworth low-pass with approximately -3 dB
at the displayed frequency across tested sample rates. The low cut is lowered from 50 to 30 Hz to retain
more low body across repeats. The saturator uses first-order antiderivative antialiasing; its linear limit
is a two-sample average inside the feedback loop. No new processing is added to the original or first echo.
An isolated 10 kHz synthetic test measured about 9 dB less selected foldback energy relative to the
fundamental than the previous direct tanh shaper. This is not an alias-free guarantee or a listening result.

Synced notes may go below the Free mode's 20 ms lower bound (down to 1 ms), so 1/64 at 240 BPM correctly
produces 15.625 ms. Existing parameter IDs and saved settings are unchanged. Test the new colour with
Feedback around 45%, Tone 2500 Hz, Wear 50%, and synced dotted eighths on a short vocal or instrument phrase.
