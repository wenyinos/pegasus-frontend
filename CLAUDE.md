# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pegasus Frontend is a cross-platform graphical frontend for browsing game libraries and launching emulators. Built with C++17 and Qt/QML, it targets Windows, Linux, macOS, Android, and embedded devices (Raspberry Pi, Odroid).

## Build Systems

The project supports both **CMake** and **qmake**. CMake is the modern path; qmake is used in CI for some platforms.

### CMake Build

```sh
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_PREFIX_PATH="/path/to/qt;/path/to/sdl2" \
  -DPEGASUS_ENABLE_APNG=ON \
  -DPEGASUS_STATIC_CXX=ON
cmake --build build
```

Key CMake options:
- `PEGASUS_USE_SDL2_GAMEPAD` (ON by default) - Use SDL2 for gamepad support
- `PEGASUS_USE_SDL2_POWER` (ON by default) - Use SDL2 for battery info
- `PEGASUS_ENABLE_APNG` (OFF by default) - Animated PNG support
- `PEGASUS_STATIC_CXX` (OFF by default) - Link stdc++ statically
- `PEGASUS_ENABLE_LTO` (ON by default) - Link-time optimizations

### qmake Build

```sh
mkdir build && cd build
qmake .. USE_SDL_GAMEPAD=1 USE_SDL_POWER=1
make
```

### Dependencies

- Qt 5.15.0+ with modules: QML, QtQuick2, Multimedia, SVG, SQL (SQLite)
- SDL2 2.0.4+ (optional, for gamepad/battery; falls back to Qt Gamepad)
- C++17 compiler

## Running Tests

```sh
# CMake
cd build
ctest --test-dir build --rerun-failed --output-on-failure

# qmake
make check
```

Tests require a display server (CI uses `xvfb-run`). Tests are in `tests/backend/` and cover: API, config files, model classes, providers, process launcher, and utilities.

## QML Linting

```sh
find -name *.qml -exec /path/to/qt/bin/qmllint {} \;
```

## Architecture

### Source Layout (`src/`)

- **`app/`** - Application entry point (`main.cpp`), platform-specific setup, install configuration
- **`backend/`** - Core C++ logic compiled as `pegasus-backend` static library
  - `Backend.cpp` - Main orchestrator connecting frontend, API, providers, and process launcher
  - `model/` - Data models exposed to QML: `Api` (public QML interface), `Internal` (private), `gaming/` (Game, Collection, GameFile), `device/`, `keys/`, `memory/`
  - `providers/` - Pluggable data source providers (see below)
  - `parsers/` - Metadata file parsers
  - `platform/` - Platform abstraction (power management, processes)
  - `ProcessLauncher` - Game execution management
  - `FrontendLayer` - Bridge between QML frontend and backend
- **`frontend/`** - QML UI files (`main.qml`, menus, dialogs, splash screen)
- **`themes/`** - Default theme as git submodule (`pegasus-theme-grid`)
- **`qmlutils/`** - QML utility helpers

### Provider System

Providers discover and load game metadata from various sources. Each extends `providers::Provider` and implements `run(SearchContext&)`. The `ProviderManager` orchestrates scanning.

Current providers:
- `pegasus_metadata/` - Native Pegasus metadata format
- `pegasus_favorites/` - Favorites storage
- `pegasus_playtime/` - Play time tracking
- `pegasus_media/` - Media assets
- `es2/` - EmulationStation gamelist.xml
- `steam/` - Steam library
- `gog/` - GOG library
- `launchbox/` - LaunchBox (Windows only)
- `playnite/` - Playnite (Windows only)
- `logiqx/` - Logiqx XML format
- `lutris/` - Lutris
- `android_apps/` - Android applications
- `skraper/` - Skraper assets

### Data Flow

```
Frontend (QML) <-> ApiObject <-> Backend <-> ProviderManager -> Providers
                                        <-> ProcessLauncher
```

### Third-party (`thirdparty/`)

- `SortFilterProxyModel` - QML sort/filter proxy (git submodule)
- `apng/` - Animated PNG Qt plugin (optional)

## Git Submodules

```sh
git submodule update --init --recursive
```

Submodules: `lang` (translations), `src/themes/pegasus-theme-grid`, `thirdparty/SortFilterProxyModel`

## CI/CD

- **GitHub Actions**: X11 (Linux), macOS, Android builds
- **CircleCI**: Embedded platforms (RPi, Odroid), MinGW cross-compilation
- **AppVeyor**: Windows MSVC builds

CI downloads pre-built Qt and SDL2 toolchains from GitHub releases.

## Code Style

- C++17 with Qt conventions
- QML uses Qt Quick Controls
- Header guards: `#pragma once`
- Namespaces: `backend`, `model`, `providers`
