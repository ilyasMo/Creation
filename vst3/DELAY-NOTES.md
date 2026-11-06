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
automation with change gestures. Hz entries accept k/kHz; decimal comma is supported. Sync mode changes the Time fader to note divisions; clicking its readout opens note/style options. Parameter attachments keep controls synchronized with host automation.

Existing parameter IDs for output gain, delay on/sync/time/feedback/ping-pong, note division/style and
Master Mix remain stable. Feedback Tone/Wear parameter IDs are removed as requested; their old automation
will no longer apply. Older sessions use the new neutral EQ-off defaults rather than reinterpreting a
feedback filter as a whole-signal EQ. Old gain and remaining settings still restore by their existing IDs.

Tests cover note timing, mono/stereo unity, whole-path EQ and first-echo response, global dry/wet endpoints
and midpoint, wow/flutter audibility in generated signals, stereo ping-pong, 44.1/48/96/192 kHz, high-feedback
modulation/automation stress, modulation buffer boundaries, filter corners, and numeric entry parsing.
Compilation and parameter/text integration still need Windows/native checks; listening, mouse interaction,
DAW sync and saved-state acceptance should be confirmed in Live 11.


0.4.0 adds the approved paper-and-ink faceplate with vector-drawn faders, shared EQ handles and
embedded Caveat handwriting. Click the DELAY or EQ heading to switch it on/off; the underline and
small ON/OFF status show its state, without the removed checkboxes. Wow/Flutter still modulate
the delay engine. The top-right asterisk opens Output gain and Ping-pong. Fader wheels are
disabled to avoid accidental host scrolling changes. The native editor resizes with a fixed aspect
ratio. No audio parameters, saved-state IDs or processing algorithms change in this visual release.


0.4.1 fixes the Return-key text-entry refresh ordering and explicitly sets dark text and a high-contrast
selection colour for numeric editors. Fader geometry now shares the actual slider layout, with cap travel
inside the drawn rail; shared EQ handles sit clear of both frequency endpoint labels. Delay/Tape/EQ headings
use their original artwork in black when enabled and grey when bypassed, without ON/OFF text. New Tape ON
parameter gates Wow/Flutter with the existing smoothing, preserving their amounts. New instances default
Tape OFF. Older saved states without Tape ON restore it enabled if Wow or Flutter were nonzero.
The note and timing selectors below SYNC are enabled only while Sync is on. They retain the same existing
parameter IDs and work with host automation; the nested readout popup is removed.
