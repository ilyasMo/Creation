# Presets and editor state, 0.6.0

The header contains the preset selector and Save As. Default restores all factory parameter values:
Delay/Tape/EQ off, Wow/Flutter zero, Master Mix 100%, neutral EQ and 0 dB Input/Output.
Custom presets are local .afterglowpreset XML files in the user's application-data directory under
Creation Audio/Afterglow/Presets. Only Save As creates the directory. Files contain parameter values,
not editor dimensions. Duplicate names return an error; saves use a temporary file and final replace.
Invalid files are rejected before applying any values. New parameters missing from a preset use defaults.

Each processor instance owns its editor width. Height follows the existing aspect ratio. Resizing
notifies the host of a non-parameter state change. Closing/reopening an editor reuses that instance's
size; saving/restoring the host project includes width and preset identity alongside all audio values.
An already-open editor follows recalled dimensions on its UI timer. Preset loading never changes size.
The project snapshot is self-contained and does not need the custom preset file to restore audio.
Parameter edits mark the preset name with an asterisk; audio-thread listeners only set an atomic flag.
File listing, parsing and writing happen on the UI thread.

The settings drawer now contains Input and Output. The old Ping-pong parameter ID remains to preserve
existing automation and session recall, but no Ping-pong control is shown in this drawer. The new Input
parameter is appended to the layout so all earlier parameter IDs/order remain stable. Input is before
the dry/wet split, Output after the mix. Both use the existing 20 ms smoothed gain stage.

Native integration checks cover real Save As/selector controls, cross-instance file loading, Default,
duplicate and invalid files, dirty indicators, self-contained project restore, size on close/reopen,
independent instance sizes, host resize notification and actual Input/Output audio processing. Existing
numeric-entry, fader limits, EQ, note/style, Tape bypass and latency checks still run. These checks do
not replace verifying editor reopen and project recall inside Ableton Live 11.
