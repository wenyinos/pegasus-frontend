# AGENTS.md — Pegasus Frontend

C++17 + Qt/QML game launcher frontend. Cross-platform (Windows, Linux, macOS, Android, embedded).

## Build

CMake is the preferred build system. Default build type is **Release**.

```sh
git submodule update --init --recursive    # required before first build

mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_PREFIX_PATH="/path/to/qt;/path/to/sdl2" \
  -DPEGASUS_ENABLE_APNG=ON \
  -DPEGASUS_STATIC_CXX=ON
cmake --build build
```

Output binary: `build/src/app/pegasus-fe`

### Key CMake options

| Option | Default | Effect |
|---|---|---|
| `PEGASUS_USE_SDL2_GAMEPAD` | ON | SDL2 gamepad input |
| `PEGASUS_USE_SDL2_POWER` | ON | SDL2 battery info |
| `PEGASUS_ENABLE_APNG` | OFF | Animated PNG support |
| `PEGASUS_STATIC_CXX` | OFF | Static link stdc++ |
| `PEGASUS_ENABLE_LTO` | ON | Link-time optimization |

## Test

Tests require a **display server**. On headless hosts use `xvfb-run`:

```sh
xvfb-run -a ctest --test-dir build --rerun-failed --output-on-failure
```

Tests are in `tests/backend/` (unit), `tests/integration/`, and `tests/benchmarks/`. Test targets mirror the source tree structure.

## QML Lint

```sh
find . -name '*.qml' -exec /path/to/qt/bin/qmllint {} \;
```

## Build targets

CMake produces these targets (in dependency order):

| Target | Type | Description |
|---|---|---|
| `pegasus-assets` | Static lib | Compiled resources (fonts, icons, SDL2 gamepad DB) |
| `pegasus-locales` | Static lib | Compiled translations (.qm files from `lang/*.ts`) |
| `pegasus-qml` | Static lib | QML assets compiled via `qtquick_compiler_add_resources` |
| `pegasus-backend` | Static lib | Core logic: models, providers, parsers, platform |
| `pegasus-fe` | Executable | Entry point in `src/app/main.cpp` |

## Architecture

```
src/
  app/              # main.cpp → pegasus-fe binary
    main.cpp        # CLI args: --portable, --silent, --kiosk, --disable-menu-*, --disable-gamepad-autoconfig
  backend/          # Static lib pegasus-backend
    Backend.cpp     # Main orchestrator
    FrontendLayer.cpp  # QML ↔ C++ bridge (dynamic reload on game launch)
    ProcessLauncher.cpp  # Game execution management
    ScriptRunner.cpp     # Custom script execution
    AppSettings.cpp      # Settings management
    Log.cpp              # Logging
    Paths.cpp            # Filesystem paths
    PegasusAssets.cpp    # Asset loading
    model/               # QML-exposed data models
      Api.cpp            # Public QML interface (ApiObject)
      gaming/            # Game, Collection, GameFile + list models
      internal/          # Gamepad, Meta, System, Settings, LogModel
      device/            # DeviceInfo
      keys/              # Key, Keys
      memory/            # Memory
    providers/        # Pluggable game sources (see below)
    parsers/          # MetaFile (metadata), SettingsFile (config)
    platform/         # PowerCommands_* (per-platform), TerminalKbd
      AndroidHelpers.h     # Android storage, SAF, JNI
      AndroidAppIconProvider.h  # Android app icons
    types/            # Enums: AssetType(21), GamepadKeyId, GamepadButton(17), GamepadAxis(5),
                      #        KeyEventType(13), AppCloseType(4)
    imggen/           # BlurhashProvider (QQuickImageProvider for placeholder images)
    utils/            # StringHelpers, PathTools, SqliteDb, FolderListModel,
                      # CommandTokenizer, KeySequenceTools, DiskCachedNAM,
                      # QmlHelpers (QML_CONST_PROPERTY macro), StdHelpers (VEC_SORT etc.),
                      # HashMap, MoveOnly, NoCopyNoMove, FakeQKeyEvent
  frontend/         # QML UI
    main.qml          # Root QML (Window, Theme Loader, Menu, Splash)
    MenuLayer.qml     # Main menu
    SplashLayer.qml   # Splash screen
    dialogs/          # GenericOkDialog, RebootDialog, ShutdownDialog, MultifileSelector, Shade
    menu/             # MainMenuPanel, SettingsMain, GameDirEditor, GamepadEditor, KeyEditor, ProviderEditor
    messages/         # Error, NoGamesError, ThemeError
    assets/           # Gamepad button images (x360, ps), logo, progress bar
    frontend.qrc      # QML resource bundle — MUST be edited when adding new QML files
  qmlutils/         # QML utility components
    HorizontalSwipeArea.qml  # Horizontal swipe gesture detection
    AutoScroll.qml           # Auto-scroll component
    qmlutils.qrc
  themes/           # themes.qrc + pegasus-theme-grid submodule
                      #   layer_grid/    — GameGrid, GameGridItem, BackgroundImage, FavoriteHeart
                      #   layer_gameinfo/ — GamePreview, PanelLeft, PanelRight
                      #   layer_platform/ — PlatformBar, PlatformCard
                      #   layer_filter/   — FilterLayer, FilterPanel, CheckBox
                      #   assets/logos/   — 100+ platform logo SVGs
```

**Adding new QML files**: Edit `src/frontend/frontend.qrc` to include them. The build uses `qtquick_compiler_add_resources`, so QML files must be registered in the `.qrc` to be compiled.

**Translation files**: `lang/pegasus_*.ts` files are compiled to `.qm` by `qt_add_translation()` in `src/app/CMakeLists.txt`. Add new `.ts` files to the `lang/` directory (submodule).

**Data flow:** `QML Frontend ↔ ApiObject ↔ Backend ↔ ProviderManager → Providers`

### Provider system

Each provider extends `providers::Provider` and implements `run(SearchContext&)`. Registered via `pegasus_add_provider()` in CMakeLists.txt with platform guards. Each enabled provider emits a `WITH_COMPAT_<CXXID>` compile definition.

Platform-conditional:
- `launchbox/`, `playnite/` — Windows only (`PEGASUS_ON_WINDOWS`)
- `es2/` (EmulationStation) — Windows, macOS, X11, or EGLFS only
- `android_apps/` — Android only
- All others (`pegasus_metadata`, `steam`, `gog`, `lutris`, `logiqx`, `skraper`, `pegasus_favorites`, `pegasus_playtime`, `pegasus_media`) — all platforms (`PLATFORMS ALL`)

Internal providers (pegasus_*) are not listed in the optional providers printout.

### Third-party (git submodules)

- `thirdparty/SortFilterProxyModel` — linked into `pegasus-backend`
- `thirdparty/apng` — optional, enabled via `PEGASUS_ENABLE_APNG`
- `src/themes/pegasus-theme-grid` — default theme (submodule)
- `lang/` — translations (submodule)

## Code Conventions

- C++17, Qt conventions (signals/slots, `QObject` hierarchy)
- `#pragma once` for header guards
- Namespaces: `backend`, `model`, `providers`
- `AUTOMOC`, `AUTORCC`, `AUTOUIC` are all ON — do **not** manually invoke `moc`
- QML uses Qt Quick Controls
- Provider compile defs: `WITH_COMPAT_<CXXID>` per provider, `WITH_SDL_GAMEPAD`, `WITH_SDL_POWER` for SDL2 features
- Platform detection: `PEGASUS_ON_WINDOWS`, `PEGASUS_ON_MACOS`, `PEGASUS_ON_ANDROID`, `PEGASUS_ON_X11`, `PEGASUS_ON_EGLFS` (set by `PegasusTargetPlatform.cmake`)
- Use `QML_CONST_PROPERTY` / `QML_READONLY_PROPERTY` macros from `utils/QmlHelpers.h` for QML-exposed properties
- Use `VEC_SORT`, `VEC_CONTAINS`, `VEC_REMOVE_IF` macros from `utils/StdHelpers.h` for container operations
- Use `NO_COPY_NO_MOVE` / `MOVE_ONLY` macros from `utils/NoCopyNoMove.h` / `utils/MoveOnly.h` for resource management classes

## Dependencies

- Qt 5.15+: QML, QtQuick2, Multimedia, SVG, SQL
- SDL2 2.0.4+ (optional, for gamepad/battery; falls back to Qt Gamepad)
