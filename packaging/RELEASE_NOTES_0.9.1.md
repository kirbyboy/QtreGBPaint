# QtreGBPaint 0.9.1

QtreGBPaint is a small pixel-art paint program in the spirit of classic
MS Paint, with indexed palettes inspired by retro handheld consoles.

## Downloads

- `qtregbpaint_0.9.1_amd64.deb` for Debian and Ubuntu systems.
- `QtreGBPaint-0.9.1-x86_64.AppImage` for other Linux distributions.
- `SHA256SUMS-0.9.1` for download verification.

## Installation

### Debian / Ubuntu

```sh
sudo apt install ./qtregbpaint_0.9.1_amd64.deb
```

### AppImage

```sh
chmod +x QtreGBPaint-0.9.1-x86_64.AppImage
./QtreGBPaint-0.9.1-x86_64.AppImage
```

The AppImage does not need installation. Use AppImageLauncher or another
desktop-integration tool if you want it registered in the application menu and
dock.

### Verify the download

```sh
sha256sum -c SHA256SUMS-0.9.1
```

## Changes

- Added Debian and AppImage packaging for GitHub releases.
- Added AppStream metadata, desktop entry and MIME registration.
- Fixed the application and window icons so docks can associate them with the
  desktop entry on X11 and Wayland.
- Split the PaintFrontend implementation into focused translation units.
- Documented the GPL-3.0-or-later code license, the CC0-1.0 asset dedication
  and the trademark policy.

## Requirements

- Debian/Ubuntu package dependencies are resolved automatically by `apt`.
- The AppImage requires a Linux x86_64 system with SDL2 runtime support.
- The application is designed for a single window and indexed images from
  1 to 8 bits per pixel.

Source code and issue tracker:

<https://github.com/kirbyboy/QtreGBPaint>
