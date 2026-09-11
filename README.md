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

ESP Explorer AE is an F4SE plugin for Fallout 4 Steam. It renders an
in-game ImGui explorer for plugins, forms, player actions, diagnostics, logs,
themes, and localization.

> [!NOTE]
> Downloads, screenshots, and release notes are available on the Nexus Mods page.

Use the F4SE build and Address Library file matching your executable:

| Fallout 4 Steam runtime | F4SE | Address Library file |
| --- | --- | --- |
| 1.11.240 | 0.7.9 | `version-1-11-240-0.bin` |
| 1.11.221 | 0.7.8 | `version-1-11-221-0.bin` |
| 1.11.191 | 0.7.7 | `version-1-11-191-0.bin` |

The plugin accepts these three executable versions. Earlier runtimes, GOG, VR,
and unknown future updates are outside this compatibility list. Do not rename
an Address Library file from another executable version.

The Inventory tab can filter equipped items, Pip-Boy favorites, legendary items,
and quest items. Sort by Stack Weight to find heavy item groups, or use Select
Visible to select the filtered list, then open Actions beside the selection
controls for bulk operations. Right-click a column header to choose which
columns to show. The details pane offers equip/use controls and an Actions menu
for count changes, dropping, removal, and inspection in Plugin Browser.

## Repository Layout

- `src/` - plugin source code.
- `src/main.cpp` - F4SE entry point and startup sequence.
- `src/Hooks/` - D3D11 Present hook, WndProc hook, cursor/input state, and menu visibility.
- `src/GUI/` - ImGui renderer, main window, tabs, popups, shared widgets, and themes.
- `src/Core/` - detached catalog/inventory models, queries, selections and action contracts.
- `src/App/` - lifecycle, publication, settings and action services.
- `src/Game/` - engine readers and game-task action execution.
- `src/Config/` - INI-backed settings.
- `src/Localization/` - language loading, fallback behavior, and font atlas support.
- `src/Input/` - gamepad polling and overlay keyboard integration.
- Logging uses CommonLib/F4SE's `REX::*` logging macros and writes to the standard plugin log under `Documents/My Games/Fallout4/F4SE`.
- `dist/lang/` - shipped language `.ini` files.
- `dist/fonts/` - shipped runtime fonts.
- `dist/themes/` - shipped theme `.ini` files.
- `nexus/` - Nexus page assets and description text.
- `lib/commonlibf4/` - CommonLibF4 dependency.

## Build

Use a recursive repository checkout, xmake 3.0 or newer, and the MSVC C++
toolchain. Build from the repository root with xmake:

```powershell
xmake f -m release -a x64
xmake
```

The release DLL is written to:

```text
build/windows/x64/release/ESPExplorerAE.dll
```

Release builds remain optimized and produce a matching `ESPExplorerAE.pdb`.
Warnings in the plugin target are treated as build errors. The existing
`releasedbg` configuration is also supported:

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

Builds do not automatically install into the game. To deploy a validated build,
configure `XSE_FO4_GAME_PATH` or `XSE_FO4_MODS_PATH`, then run `xmake install`.

## Packaging

Package the current build with its language files, fonts, and themes:

```powershell
xmake package
```

The archive `build/packages/ESPExplorerAE-<version>.zip` contains the DLL under
`Data/F4SE/Plugins` and the language, font, theme, and license files under
`Data/Interface/ESPExplorerAE`. Font binaries and redistribution notices are
included in the repository; no font download or conversion step is needed.

Menu icons use the bundled [Lucide](https://lucide.dev/) icon font through ImGui.
Its ISC and Feather MIT notices are included under `fonts/licenses`.

Keep the matching `ESPExplorerAE.pdb` from the build output for diagnostics.

Packaging copies these files from `dist` into the game's `Data` layout. The
plugin loads them from `Data/Interface/ESPExplorerAE/{lang,fonts,themes}`.

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

Relative `sFontFiles` entries resolve inside `Data/Interface/ESPExplorerAE/fonts`.
`sGlyphRanges` can include ImGui preset ranges such as `default`,
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
