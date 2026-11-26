# ThisIs395 VJ

A cross-platform, GPU-accelerated music visualization and VJ production suite powered by projectM, SDL2, Qt6, FFmpeg, and an increasingly concerning amount of GitHub Actions compute time.

This is a fork of the [projectM SDL2 frontend](https://github.com/projectM-visualizer/frontend-sdl2).

## Features

- Real-time MilkDrop preset visualization.
- Native Qt6 control center with a separate visualizer window.
- FFT-reactive PNG sprites with adjustable position, scale, rotation, opacity, and audio sensitivity.
- Preset sequencing and timeline-based production.
- Experimental offline video rendering with FFmpeg.
- Native system audio capture on macOS 14.4 and newer.
- A green interface, because blue was deemed insufficiently green.
- Display selection in the Visualizer launch settings, with SDL-reported monitor names, resolution and refresh rate. Move switches the running visualizer to the selected monitor and attempts to preserve fullscreen; Refresh updates the list after display changes. The chosen monitor is remembered for future launches.
- Persistent bottom status-bar FPS readout, measured from completed frames and refreshed once per second while the renderer is connected.
- Live mesh X/Y controls under Visualizer → Rendering quality. Apply & save adjusts the running renderer and saves the same user configuration used by the SDL settings UI. The default is 96 × 54; lower values reduce per-point CPU work without reducing the output resolution.

Development builds: [GitHub Actions](https://github.com/thisis395/projectMSDL-ThisIs395-VJ-Fork/actions). Use the unified suite workflow for downloadable development artifacts. These are not stable releases.

Technical documentation: [Native control center](docs/ThisIs395_Qt_Native_UI.md).

## Portable favorites

Star presets or PNG sprites in the playlist editor; use the Favorites tab to browse saved copies, cue presets, load sprites (native UI), or add them to sequences. Star/Unstar writes immediately. No entire preset pack is scanned or copied for this feature.

```text
ThisIs395-VJ.exe (or the Linux executable / macOS projectM.app)
Favorites/
  Presets/
  Sprites/
```

The files themselves are copied, not shortcuts. Move the Favorites folder alongside your next executable or app bundle. macOS stores it next to the .app, not inside its signed contents. Name collisions retain the original filename in numbered subfolders. Unstarring deletes the saved copy, never the external original; if a show references that copy, keep it starred until the show no longer needs it. Presets may rely on additional texture files: keep their texture collection available when moving the suite. Installed locations without write permissions show a storage error; use a writable portable installation.

## Developer Disclosure

Development and hands-on testing of the native UI were conducted on Windows 11 26H2, using what the developer considers the shittiest Razer known to mankind.

The developer reserves and exercises his sole and inalienable right to use a Mac 99% of the time. Unfortunately, circumstances involving a raccoon temporarily prevented the exercise of said right.

As a result, Windows was used not out of preference, but out of necessity. The Razer was an unwilling participant. The raccoon has declined to comment.

The use of Windows during development does not constitute an endorsement of Windows.

## Preset Pack Warning: Your SSD May Start Screaming

> [!WARNING]
> Large MilkDrop/projectM preset collections can contain hundreds of thousands of tiny files. Extracting, copying, scanning, or indexing them can cause prolonged storage activity, high I/O latency, and a temporarily unresponsive system. An SSD is strongly recommended, but even an SSD is not immune to suffering.

For reference, the developer's larger historical collection contained approximately **283,000 files and nearly 28 GB** of presets and textures. File transfers involving this collection took approximately **22 hours on both Windows and macOS**. The older test machine was a 2014 MacBook Pro transferring to a SanDisk Extreme Portable SSD over USB 3.0.

For comparison, nearly 300 GB of other data was restored from that 2014 MacBook Pro to a newer Intel i9 MacBook Pro in just 26 minutes. The preset collection was considerably smaller in bytes, but apparently much larger in emotional damage.

The first projectM GUI launch with the large collection took **28 minutes and 1 second**. A later launch following a cold reboot took approximately 15 seconds. The SSD, according to the developer, was still screaming.

These are historical observations, not controlled benchmarks or guaranteed installation times. The more recent collection contains roughly 135,000 files and occupies around 4 GB uncompressed. Preset counts and sizes vary by pack.

**God help you if you use a Toshitba HDD on USB 2.0.** Between mechanical seek latency, USB 2.0 bandwidth, and hundreds of thousands of small files, the bottleneck may become your remaining natural lifespan. Seriously, use an SSD over USB 3.0 or faster if possible.

### Recommended precautions

- Allow sufficient free space for both the compressed archive and the extracted collection.
- Start with a smaller preset selection if you do not need the entire library.
- Avoid extracting or scanning the pack while rendering video, processing audio, or performing other storage-intensive work.
- If the disk is saturated, let the operation finish. Check storage activity and free space before assuming the application has frozen.
- If copying the entire collection between computers, consider transferring a single archive and extracting it locally rather than transferring hundreds of thousands of individual files. This does not eliminate the extraction cost.
- Do not repeatedly force traditional defragmentation on an SSD. Use the operating system's normal SSD optimization facilities.

The sound of an SSD screaming is figurative. If your SSD is literally screaming, please investigate other components before opening a GitHub issue.

**No SSDs were intentionally harmed in the development of ThisIs395 VJ. Unintentional emotional damage remains under investigation.**

## Development Philosophy

- If it compiles, it might work.
- If it passes CI, it might still not work.
- If it works on Linux, Windows reserves the right to disagree.
- If it works on Windows, the sprite might be upside down.
- If everything works, stop touching it.

## The U L T I M A T E  R E G R E S S I O N™

During development of the native control center, the project exceeded 700 GitHub Actions workflow runs.

The Sprite Manager underwent extensive architectural revisions, texture-loading improvements, IPC repairs, and cross-platform regression testing.

Among the discoveries:

- A successfully created sprite can actually be an invisible 1×1 texture.
- POCO's default Windows argument parsing can silently ignore the renderer's command-line options.
- A passing Linux integration test does not constitute proof that Windows works.
- Vertically flipping an image that does not need to be flipped results in a vertically flipped image.

That last discovery required another cross-platform compilation cycle.

## Apple Silicon

**Planned release note: ADDED APPLE SILICON AT THE EXPENSE OF SILICON.**

Despite being developed with modern macOS support in mind, the initial native macOS builds exclusively targeted Intel processors.

This was not a deliberate preservation initiative for historical computing architectures.

We simply forgot.

Native ARM64 and Intel build jobs now compile the unified executable. The new macOS packaging changes still need a successful CI run and hands-on testing before the release note becomes official.

## Frequently Asked Questions

**Why are there two processes if this is a single application?**

The Qt control center and SDL renderer operate in separate processes launched from the same executable. This keeps their respective event loops and rendering responsibilities isolated.

**Why is the Sprite Manager so extensively tested?**

Historical reasons.

**Why does the repository have so many workflow runs?**

Cross-platform compatibility is important. Apparently, so is heating the atmosphere.

**Does passing CI guarantee that everything works?**

No. We learned this empirically.

**Can I run this on my Intel Mac?**

The intended macOS deployment target is Sequoia 15; the minimum deployment version of the current packaged binaries has not yet been verified. Performance depends on your GPU, rendering resolution, and preset complexity.

**Is Apple Silicon supported?**

A native ARM64 build job exists and has compiled the application; distributable Apple Silicon and Intel bundles are pending packaging verification. Rosetta compatibility has not been formally verified.

## Environmental Impact

No actual environmental impact assessment has been conducted.

However, the maintainers extend their sincerest condolences to the GitHub Actions runners involved in the development of the Sprite Manager.

## Acknowledgments

- The projectM developers.
- The original MilkDrop developers.
- The Qt, SDL2, POCO, and FFmpeg communities.
- GitHub Actions, for its extraordinary patience.
- Every GPU that participated in testing.
- The PNG that was eventually rendered correctly (orientation fix pending verification).

---

*ThisIs395 VJ: Making music visible, one regression at a time.*

---

## Building the unified VJ suite

The unified application uses Qt6 Widgets and the projectM 4.2+ SDK. See the [unified suite workflow](.github/workflows/unified-suite.yaml) for the pinned libprojectM revision and platform-specific dependency setup.

For the unified build, configure with `-DENABLE_QT_NATIVE_UI=ON -DENABLE_VJ_SINGLE_BINARY=ON` in addition to the appropriate CMake dependency paths. FFmpeg is required for offline video export.

The following original upstream build instructions are retained for the standalone SDL frontend.

## Building from source

### Build and install libprojectM

First, [build libprojectM](https://github.com/projectM-visualizer/projectm/wiki/Building-libprojectM) or get it via your
favorite dependency management/packaging tool. For testing, you can install libprojectM somewhere inside your
home/development directory using `CMAKE_INSTALL_PREFIX`, then pass the same install path to the frontend-sdl2 build
using `CMAKE_PREFIX_PATH`. Please refer
to [CMake's documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_PREFIX_PATH.html) for details.

### Dependencies

This project requires third-party libraries in addition to libprojectM's core library dependencies:

- SDL2 (version 2.0.16 or higher)
- POCO (recommended version 1.12 or higher, minimum is 1.9.x)
- Freetype 2 (optional, will provide better looking UI text)

**Important**: projectMSDL will _not compile_ against Poco versions from 1.10.0 up to 1.11.1, as these versions of Poco
include a serious issue that causes the application to crash. Either use Poco 1.9.x, or upgrade to 1.11.2 or higher.

Depending on your needs, you can either build them yourself or install them using your favorite package manager. Here
are some examples for the three major desktop platforms:

```shell
sudo apt install libsdl2-dev libpoco-dev libfreetype-dev cmake  # Debian/Ubuntu Linux
brew install sdl2 poco freetype  # macOS
vcpkg install sdl2 poco freetype # Windows, should be pulled in automatically via vcpkg.json
```

### Configure and build projectMSDL

After cloning or updating the Git repository, always remember to initialize and update the submodules as well. this is
not required when building from a release tarball or ZIP.

```shell
# Newer git versions also support "git submodule update --init" to perform both step in a single command.
git submodule init
git submodule update
```

If all dependencies are in the CMake and/or the system search directories, you can configure and build the application
with these commands, executed from the source dir:

```shell
mkdir cmake-build
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build --config Release
```

You can optionally add the `--parallel` argument with the number of CPU cores to use to the build command to speed up
the build.

If your dependencies are in different locations than the default search paths, or you're cross-compiling, you'll need to
add more options like `CMAKE_PREFIX_PATH`. Covering all CMake options is out of the scope of this document. Please read
the [Mastering CMake guide](https://cmake.org/cmake/help/book/mastering-cmake/index.html) and
the [CMake documentation](https://cmake.org/cmake/help/latest/) for more information.

The above command will use CMake's default build file generator for your current platform and build the project in
Release (optimized) configuration. If the build was successful, you should have an executable in the build directory. On
Windows, you may need to specify the correct Visual Studio generator and architecture manually.

### Install projectMSDL

While you can run projectMSDL directly from the build directory, it's recommended to install the project. This will copy
all required files, including a default configuration file, into the installation dir.

You can set the installation target path in the first CMake command which configures the build. The `install` target
will then copy everything under this directory:

```shell
mkdir cmake-build
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/install/dir
cmake --build cmake-build --config Release --target install
```

### Run directly from the build dir

You should have a directory of visual presets you wish to use. You can fetch a giant trove of curated
presets [here](https://github.com/projectM-visualizer/presets-cream-of-the-crop). You will also need textures (images)
used in many presets. The projectM team has
assembled [a pack of textures](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack), which covers the
needs of most presets.

If you want to run the executable from the build dir, you'll need to pass any non-default settings via arguments. You
can also create a user configuration file in your user's home directory. Depending on the platform, this will be:

- Windows: `%APPDATA%\projectM\projectMSDL.properties`
- Linux:
    - If `XDG_CONFIG_HOME` env var is non-empty: `$XDG_CONFIG_HOME/projectM/projectMSDL.properties`
    - Otherwise: `~/.config/projectM/projectMSDL.properties`
- macOS: `~/Library/Preferences/projectM/projectMSDL.properties`

You can copy the [config file template](src/resources/projectMSDL.properties.in) there and change anything in `@@`.

Depending on the build system, you'll find the projectM executable in `cmake-build/src/`, or a subdirectory with the
name of your build type (`Release`, `Debug` and so on).

If you're not using a config file, provide the presets and texture paths you wish to use when starting projectMSDL:

```shell
cmake-build/src/projectMSDL --presetPath /path/to/presets-cream-of-the-crop --texturePath /path/to/textures
```

Press ESC to toggle the UI.

## System-Specific CMake Examples

The following examples show how to configure and build projectMSDL with CMake on the different supported platforms.

### Windows

To generate a Visual Studio 2022 project for Win64 and build for Release, with then option to also compile a Debug build
from within the generated solution:

```shell
mkdir cmake-build
cmake -G "Visual Studio 17 2022" -A x64 -S . -B cmake-build -DCMAKE_CONFIGURATION_TYPES=Debug,Release
cmake --build cmake-build --config Release
```

### Linux

To generate a UNIX Makefile project build for Release:

```shell
mkdir cmake-build
cmake -G "Unix Makefile" -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

To generate a Ninja project build for Release:

```shell
mkdir cmake-build
cmake -G "Ninja" -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

### macOS

To generate an Xcode project and build for Release:

```shell
mkdir cmake-build
cmake -G Xcode -S . -B cmake-build -DCMAKE_CONFIGURATION_TYPES=Debug,Release
cmake --build cmake-build --config Release
```

#### macOS: Full build from source (including libprojectM)

Development Intel macOS binaries are available through the unified suite workflow; the following instructions build the original standalone frontend from source.

**Prerequisites**

```shell
xcode-select --install
brew install cmake ninja sdl2 poco freetype git
```

**1. Build and install libprojectM**

```shell
git clone --recurse-submodules https://github.com/projectM-visualizer/projectm.git
cd projectm && mkdir cmake-build
cmake -G Ninja -S . -B cmake-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/dev/projectm-install \
  -DENABLE_PLAYLIST=ON \
  -DBUILD_SHARED_LIBS=ON
# Optional: add -DENABLE_MACOS_FRAMEWORK=ON to build as a .framework bundle
# (libprojectM 4.2+ only — easier to link against in Xcode projects)
cmake --build cmake-build --parallel
cmake --install cmake-build
cd ..
```

**2. Build frontend-sdl2**

```shell
git clone --recurse-submodules https://github.com/projectM-visualizer/frontend-sdl2.git
cd frontend-sdl2 && mkdir cmake-build
cmake -G Ninja -S . -B cmake-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=~/dev/projectm-install \
  -DCMAKE_INSTALL_PREFIX=~/dev/projectm-app
cmake --build cmake-build --parallel
cmake --install cmake-build
```

`projectM.app` will be in `~/dev/projectm-app/`. Edit
`projectM.app/Contents/Resources/projectMSDL.properties` to set your preset and texture paths, then launch the app.
