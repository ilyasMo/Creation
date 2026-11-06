# Instant visual prototype

Open Afterglow.html in Chrome or Edge. No compilation or installation is needed. For design changes,
edit its HTML/CSS in Visual Studio, save, then refresh the browser. Load audio and textures locally.

This is the browser prototype with its existing JavaScript sound engine. It does not execute the native
C++ delay engine. Use the VST3 or Standalone build to evaluate native sound, host automation, tempo sync,
and native interface behavior. The native editor is currently a basic parameter editor; the full artwork
has not been ported yet.

In Visual Studio: Git > Pull; find browser/Afterglow.html; use Open Containing Folder, then open the file
in your browser. This folder is separate from the CMake project in vst3/.
