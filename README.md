# SLEER

Open source dithering app for images, audio & video.

The current baseline is the image ditherer: a single-window desktop app that
reduces an image to a colour palette using error diffusion, ordered, random or
plain colour reduction. Audio and video are not implemented yet.

## Features

- **Palette dithering** — every pixel is mapped onto the selected palette; error
  diffusion works in RGB, so it handles colour palettes and plain black/white.
- **Four dithering modes**: error diffusion, ordered (Bayer 2/4/8/16), random
  noise, and colour reduction only.
- **Nine diffusion kernels**: Floyd-Steinberg, False Floyd-Steinberg,
  Jarvis-Judice-Ninke, Stucki, Burkes, Sierra 3, Sierra 2, Sierra Lite and
  Atkinson, with optional serpentine scanning.
- **Presets**: Black & White, 2-Bit Gray, RGB, CMYK, 3-Bit, 6-Bit RGB, Game Boy,
  Teletext, Apple II, Commodore 64, ZX Spectrum, Vaporwave and Hacker.
- **Palette editor** — click a swatch, drag its R/G/B sliders, add or remove
  colours, or extract a palette from the image with median cut.
- **Brightness bias** to push the pattern lighter or darker, live preview, zoom
  and pan.
- **Compare slider** that wipes the source over the dithered result, so you can
  judge a setting without toggling views.
- **Settings cog** in the top bar, opening a placeholder panel where renderer,
  input and palette options will go.
- **Startup splash**: the window opens on a CLI-style banner - block SLEER
  wordmark with a drop shadow, welcome and version lines, corner frame and a
  three-headed snake mascot - which holds for a moment and then fades into the
  app.
- **Smooth sliders**: dithering runs on a background thread. While settings are
  changing it renders a downscaled proxy; a full-resolution pass is queued once
  you stop (and again for every save).
- **Loads** PNG, JPEG, BMP, TGA, GIF, PSD, HDR and PNM; saves RGB PNG. Drop a
  file onto the window to open it, or paste one from the clipboard with `Ctrl+V`
  (or the Paste button).

## Build

Requirements: a C11 compiler and SDL3 (3.4 or newer). Image decoding uses the
vendored `stb_image` / `stb_image_write` headers in `third_party/`.

### Windows (MinGW-w64)

```bash
./tools/fetch_deps.sh          # downloads SDL3 into .deps/sdl3
mingw32-make                   # `make` works too
./build/sleer.exe
```

The build copies `SDL3.dll` next to the executable, so `build/` stays
self-contained.

### Linux

```bash
sudo apt install build-essential libsdl3-dev   # or your distro's equivalent
make SDL3_DIR=/usr
./build/sleer
```

### macOS

```bash
brew install sdl3
make SDL3_DIR="$(brew --prefix sdl3)"
./build/sleer
```

`make` without `SDL3_DIR` looks in `.deps/sdl3`.

## Usage

```bash
sleer [image.png]                     # open an image straight away
sleer --smoke-test in.png out.png     # scripted self-check, see Testing
```

The left panel holds the presets and the palette editor; the right panel holds
the dithering mode, kernel, matrix size, the compare slider and the bias.
"Viewing: dither" in the top bar switches to the unprocessed source.

Drag the compare slider (or use it while tweaking settings) to wipe the source
over the dithered result on the canvas: 0% shows only the dither, 100% only the
source. The split can also be grabbed and dragged directly on the image - the
cursor turns into a resize arrow over the handle.

| Shortcut | Action |
| --- | --- |
| `Ctrl+O` | Open image |
| `Ctrl+S` | Save dithered PNG |
| `Ctrl+V` | Paste an image from the clipboard |
| `1` … `9` | Pick a diffusion kernel |
| `T` | Cycle dithering mode |
| `[` / `]` | Bias down / up |
| `R` | Reseed the random pattern |
| `Space` | Toggle source vs dithered view |
| `F` | Fit image to window |
| `+` / `-` | Zoom in / out |
| Mouse wheel | Zoom the canvas, or scroll the swatch grid |
| Drag on canvas | Pan |
| `Esc` | Quit |

Saving always renders a full-resolution pass, even when the preview is a proxy.
Alpha is ignored; the output is an RGB PNG.

## Performance

On a 1024×768 image the 900 px preview takes ~11–14 ms per pass and the full
resolution pass ~14–17 ms; a 1920×1080 image takes ~40–57 ms for a full pass
depending on the kernel. The palette lookup table (32×32×32, built once per
palette) is what keeps larger palettes cheap: 512 colours cost ~19 ms to build,
small palettes well under 1 ms.

Because the work happens on a worker thread with a proxy preview, dragging a
slider never blocks the window.

## Testing

```bash
make test     # engine, palette and image tests, no SDL needed
make smoke    # renders one frame headlessly and saves build/smoke.png
```

`--smoke-test <in> <out>` boots the real window, opens the image, clicks the
Game Boy preset through the normal input path, drags the bias slider, waits for
both the proxy and full-resolution passes, and writes a screenshot. It fails the
run if any of those steps stop working, which makes it a quick check on a new
machine.

## Layout

```
src/dither.c   palette dithering engine: kernels, ordered/random, LUT quantizer
src/palette.c  presets, gray/RGB palette generators, median-cut extraction
src/image.c    image loading, RGB PNG export, proxy downscaling
src/worker.c   background dither thread with proxy/full-resolution requests
src/panels.c   palette panel, dither panel, canvas, top and status bars
src/ui.c       immediate-mode buttons, sliders, toggles, text
src/main.c     window, event loop, dialogs, saving, smoke test
tests/         headless engine, palette and image tests
third_party/   vendored stb headers
tools/         dependency fetch script
```

The engine and palette modules are plain C with no SDL or file I/O, so a CLI
batch tool or an audio/video pipeline can reuse them.

## Limitations

- The proxy preview is drawn scaled up during a drag, so fine detail resolves
  only after you stop.
- Manual palette edits are not saved between sessions, and presets are built in
  (no custom preset files yet).
- Audio and video dithering from the project description are not implemented.
- The window uses SDL's built-in debug font, so the UI text is intentionally
  blocky.

## Licences

SLEER is MIT licensed (see `LICENSE`). Third-party code: SDL3 (zlib licence),
`stb_image.h` / `stb_image_write.h` (public domain / MIT, see the header
comments).
