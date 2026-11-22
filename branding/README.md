# App icon generation

The projectM artwork supplied for ThisIs395 VJ is stored in `branding/projectm-icon.png` (a size-optimized copy of the supplied image).

The existing build and packaging hooks use `src/resources/icons/icon.ico` for Windows, `icon.icns` for macOS, and the size-specific PNGs plus `icon_scalable.svg` for Linux. All GitHub Build Check and release jobs regenerate these assets before compiling.

To generate the same icons locally **before configuring/building CMake**:

```sh
python -m pip install Pillow
python tools/generate_icons.py
```

The generated files replace the checkout's existing upstream icon assets. The SVG contains the raster artwork embedded in an SVG container; it is **not** a vector trace. The source image is 256×256, so larger macOS sizes are upscaled. For best results in a later release, replace the master with higher-resolution original artwork.
