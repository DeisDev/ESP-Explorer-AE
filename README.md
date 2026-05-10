# ESP Explorer AE

<p align="center">
  <a href="https://github.com/DeisDev/ESP-Explorer-AE/forks"><img src="https://img.shields.io/github/forks/DeisDev/ESP-Explorer-AE?style=social" alt="GitHub forks"></a>
  <a href="https://github.com/DeisDev/ESP-Explorer-AE/stargazers"><img src="https://img.shields.io/github/stars/DeisDev/ESP-Explorer-AE?style=social" alt="GitHub stars"></a>
  <a href="https://github.com/DeisDev/ESP-Explorer-AE/watchers"><img src="https://img.shields.io/github/watchers/DeisDev/ESP-Explorer-AE?style=social" alt="GitHub watchers"></a>
  <a href="https://github.com/DeisDev/ESP-Explorer-AE/issues"><img src="https://img.shields.io/github/issues/DeisDev/ESP-Explorer-AE?style=social" alt="GitHub issues"></a>
  <a href="https://github.com/DeisDev/ESP-Explorer-AE/pulls"><img src="https://img.shields.io/github/issues-pr/DeisDev/ESP-Explorer-AE?style=social" alt="GitHub pull requests"></a>
</p>

<p align="center">
  <a href="https://www.nexusmods.com/fallout4/mods/102223?tab=description"><img src="https://nexus-mods.github.io/NexusMods.App/Nexus/Images/Nexus-Icon.png" alt="ESP Explorer AE on Nexus Mods" width="48"></a>
</p>

ESP Explorer AE is an F4SE plugin for Fallout 4 AE / Next-Gen. It renders an
in-game ImGui explorer for plugins, forms, player actions, diagnostics, logs,
themes, and localization.

> [!NOTE]
> This repository is primarily for development. End-user downloads, screenshots,
> and release notes belong on the Nexus Mods page.

## Repository Layout

- `src/` - plugin source code.
- `src/main.cpp` - F4SE entry point and startup sequence.
- `src/Hooks/` - D3D11 Present hook, WndProc hook, cursor/input state, and menu visibility.
- `src/GUI/` - ImGui renderer, main window, tabs, popups, shared widgets, and themes.
- `src/Data/` - load-order, plugin, form, and category cache building.
- `src/Config/` - INI-backed settings.
- `src/Localization/` - language loading, fallback behavior, and font atlas support.
- `src/Input/` - gamepad polling and overlay keyboard integration.
- `src/Logging/` - file logging under `Documents/My Games/Fallout4/F4SE`.
- `dist/lang/` - shipped language `.ini` files.
- `dist/fonts/` - shipped runtime fonts.
- `dist/themes/` - shipped theme `.ini` files.
- `nexus/` - Nexus page assets and description text.
- `Scripts/` - local helper scripts.
- `lib/commonlibf4/` - CommonLibF4 dependency.

## Runtime Startup

Initialization order matters because UI text, fonts, hooks, and cached game data
depend on earlier systems being ready.

1. `src/main.cpp` loads config.
2. Logging is initialized from config.
3. Localization is loaded for the configured language.
4. F4SE messaging is registered.
5. Hooks are installed.
6. `DataManager` refreshes cached game data.

Features that depend on localized strings or font coverage should assume config
and language setup have completed before the UI is first drawn.

## Build

Build from the repository root with xmake:

```powershell
xmake f -m release -a x64
xmake
```

The release DLL is written to:

```text
build/windows/x64/release/ESPExplorerAE.dll
```

For a debug-friendly release build:

```powershell
xmake f -m releasedbg -a x64
xmake
```

For a clean release rebuild:

```powershell
xmake clean
xmake f -m release -a x64
xmake
```

## Packaging

Package the current build with the shipped runtime assets:

```powershell
xmake package
```

The package is written under `build/packages/` and includes:

- the built DLL;
- `dist/lang/*.ini` under `Data/Interface/ESPExplorerAE/lang`;
- `dist/fonts/*.ttf` under `Data/Interface/ESPExplorerAE/fonts`;
- `dist/themes/*.ini` under `Data/Interface/ESPExplorerAE/themes`.

Run packaging after changes to install layout, shipped languages, fonts, themes,
or release metadata.

## Runtime Assets

At runtime, the plugin loads user/game-install assets first and falls back to
the development `dist` folders.

- Languages: `Data/Interface/ESPExplorerAE/lang`, then `dist/lang`.
- Fonts: `Data/Interface/ESPExplorerAE/fonts`, then `dist/fonts`.
- Themes: `Data/Interface/ESPExplorerAE/themes`, then `dist/themes`.

This lets packaged installs use the normal Fallout 4 `Data` layout while local
development can run from the repository assets.

## Core Modules

`DataManager` owns heavy game-data enumeration. It builds cached plugin, form,
and category views from `RE::TESDataHandler` and `RE::TESForm::GetAllForms()`.
UI code should read cached data rather than casually forcing refreshes.

`MainWindow` owns top-level UI state, tab orchestration, modal popups, and the
status bar. Most feature work lands in the relevant tab under `src/GUI/Tabs/`
or in shared widgets under `src/GUI/Widgets/`.

`ImGuiRenderer` owns ImGui context setup, theme application, and font rebuild
processing. Language or font changes should preserve the existing sequence used
by `SettingsTab`: save config, reload `Language`, then request a font rebuild
through `FontManager`.

`Config` persists favorites, filters, window state, theme settings, input
settings, and other plugin options.

## Localization

Localization is part of feature completeness.

- Route every user-facing label, button, menu item, tooltip, popup, status text,
  and section header through the existing localization helpers.
- Add new keys to `dist/lang/en.ini` first.
- Mirror every new key across all shipped language files in `dist/lang/`.
- If a real translation is not available, copy the English value rather than
  omitting the key.
- Keep section/key naming consistent with existing sections such as `General`,
  `Settings`, `Items`, `NPCs`, `Objects`, `Spells`, `Player`, `PluginBrowser`,
  `FormDetails`, and `Logs`.

English is the reference language. Some shipped translations were created with
LLM assistance and may contain mistakes.

Language files may also declare display name and font coverage:

```ini
[Language]
sName = Polish
sFontFiles = NotoSans-Regular.ttf, MyPolishFont.ttf
sGlyphRanges = default, cyrillic
```

`sFontFiles` is resolved from `Data/Interface/ESPExplorerAE/fonts` first, then
`dist/fonts`. `sGlyphRanges` can include ImGui preset ranges such as `default`,
`cyrillic`, `japanese`, `chinese`, `chinese-full`, `korean`, `thai`, or
`vietnamese`.

## Themes

Themes are data-driven `.ini` files. Shipped themes live in `dist/themes` and
are packaged to `Data/Interface/ESPExplorerAE/themes`.

When adding or changing a theme:

- start from an existing file;
- keep the same key structure;
- use readable foreground, accent, disabled, and background colors;
- test dense tables, disabled text, popups, and different font sizes;
- run `xmake package` so the packaged layout includes the theme.

## Validation

There is no dedicated automated test suite in this repository.

Use the available checks:

- run `xmake` for compile verification;
- run `xmake package` for packaging changes;
- test in-game for UI, input, hook, player-action, or rendering changes;
- inspect all shipped language files for UI text changes.

High-risk areas include hook installation, input interception, font atlas rebuild
timing, save-affecting player actions, and `DataManager::Refresh()` behavior.

## Contributors

<a href="https://github.com/DeisDev/ESP-Explorer-AE/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=DeisDev/ESP-Explorer-AE" alt="ESP Explorer AE contributors" />
</a>

## Contributing

For contribution expectations, see [contributing.md](contributing.md).

Short version:

- keep pull requests focused;
- follow nearby file style;
- avoid speculative rewrites;
- keep data enumeration separate from UI rendering;
- prefer existing helpers for localization, filters, context menus, form actions,
  and shared widgets;
- build locally before submitting.

AI-assisted code is allowed, but contributors are responsible for understanding,
testing, and explaining their changes.

## Developer Notes

- This project is data-driven where practical. Before hardcoding game data,
  check whether Fallout 4, F4SE, or CommonLibF4 exposes the data already.
- Nexus page copy lives in `nexus/description.bbcode` and should stay separate
  from developer documentation.

## Credits

- F4SE team.
- CommonLibF4 / libxse contributors.
- ImGui contributors.
- SimpleIni contributors.
- ESP Explorer AE contributors.

## License

ESP Explorer AE source code is licensed under the GNU General Public License
v3.0 only. Non-code assets, including images, screenshots, promotional artwork,
mod page artwork, logos, icons, and other media assets, are all rights reserved
unless a file states otherwise. See [LICENSE](LICENSE).
