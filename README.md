# vice32.js
Emscripten port of the Commodore 64 emulator VICE, version 3.2.

## Improvements over vice.js 2.4
* Partially working menus (more improvements to come)
* ReSID support
* Built-in keymaps, identical cross-browser
* * VICE.js 2.4 used a custom keymap to compensate for Emscripten SDL1's brokenness.
* * Defaults to symbolic, can be set to positional in the menu
## Build instructions
1. Ensure a `js` directory appears for the output
1. Overwrite files in `emsdk` install with `emscripten_fixes` versions.
1. Run `autogen.sh` to produce configure files.
1. Run `full_build.sh` (to create permanent side-effects and to build LLVM IR "executables")
1. Run `build-x64.sh`
1. For future runs, just use `emmake make` and `build-x64.sh`. 
