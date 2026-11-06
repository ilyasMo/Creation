# Native delay 0.3.0

Focus remains the native VST3 running in Ableton Live 11. This release replaces the Feedback Tone and
Feedback Wear controls with Wow, Flutter and independent Low Cut / High Cut sliders. Removed Wear
saturation is no longer in the feedback loop. Other effect modules are not implemented in C++ yet.

Wow is a blend of slow 0.45/0.17 Hz delay modulation, up to 3 ms depth; Flutter uses 7.5/11.31 Hz components,
up to 0.3 ms. Amount changes are smoothed and stereo modulation is linked. Nominal delay time remains
host-synced, but enabled wow/flutter intentionally vary the instantaneous read time. Modulated reads are
bounded with extra ring-buffer margin, including at extreme synced times.

EQ is AFTER the delay stage, shaping the entire processed signal including core audio and the first echo.
It has an independent EQ ON switch. Low Cut (high-pass) and High Cut (low-pass) each sweep 20 Hz–20 kHz,
using smoothed Butterworth TPT filters. Cutoffs can cross. Master Mix crossfades the untouched original
against the complete processed path including EQ. All processing starts OFF, Master Mix at 100%, gain 0 dB.

A native custom editor replaces the generic editor. All continuous numeric readouts open for text editing
on double-click; Enter commits, Escape cancels. Values clamp/snap to parameter ranges and notify host
automation with change gestures. Hz entries accept k/kHz; decimal comma is supported. Sync mode disables
the unused free-time control. Parameter attachments keep controls synchronized with host automation.

Existing parameter IDs for output gain, delay on/sync/time/feedback/ping-pong, note division/style and
Master Mix remain stable. Feedback Tone/Wear parameter IDs are removed as requested; their old automation
will no longer apply. Older sessions use the new neutral EQ-off defaults rather than reinterpreting a
feedback filter as a whole-signal EQ. Old gain and remaining settings still restore by their existing IDs.

Tests cover note timing, mono/stereo unity, whole-path EQ and first-echo response, global dry/wet endpoints
and midpoint, wow/flutter audibility in generated signals, stereo ping-pong, 44.1/48/96/192 kHz, high-feedback
modulation/automation stress, modulation buffer boundaries, filter corners, and numeric entry parsing.
Compilation and parameter/text integration still need Windows/native checks; listening, mouse interaction,
DAW sync and saved-state acceptance should be confirmed in Live 11.
