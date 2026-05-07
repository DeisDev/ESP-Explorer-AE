# ESP Explorer AE

ESP Explorer AE is an in-game ESP/ESM/ESL plugin and record explorer for Fallout 4 Anniversary Edition.

It lets you browse the plugins, forms, and records in your load order without leaving the game. Inspect record details, search across categories, spawn items or NPCs, add spells and perks, manage your inventory, or teleport to cells while you play.

The focus is speed, useful detail, and practical tools instead of bloated menus. It is built for large load orders, works with or without the official DLCs, and includes advanced filtering, diagnostics, favorites, recent records, action history with best-effort undo, log viewing, controller support, and multi-language support.

## Features

- Browse plugins, grouped records, and your load order in-game.
- Optimized for both light and heavy load orders.
- Supports ESL, ESM, and ESP plugins.
- Dedicated tabs for Inventory, Items, NPCs, Cells, Objects, Spells, and Perks.
- Full inventory browser with category tabs, search, equipped filtering, stack details, and quick actions.
- Give, spawn, drop, remove, equip, unequip, duplicate, use, and teleport actions where applicable.
- Weapon and armor mod inspection plus attach/detach support from the inventory tab.
- Favorites, recent records, clipboard copy tools, and context-aware right-click menus across the UI.
- Plugin diagnostics and advanced record inspection.
- Rule-based advanced filters with saved rules, regex support, hidden-plugin management, and global plugin search.
- Action History with best-effort undo for supported actions.
- Optional automatic component substitution so crafting components are given as usable scrap items.
- Built-in log viewer with copy and export support.
- Multiple color presets, custom theme colors, font sizing, and optional Pip-Boy color sync.
- Basic controller support with optional gamepad navigation.
- Data-driven multi-language support.

Some shipped translations were created with LLM assistance and may still contain inaccuracies. English is the reference language.

Want to make your own translation? Copy `en.ini` into `Data/Interface/ESPExplorerAE/lang`, rename it to your language code, translate the values, and optionally add `sName`, `sFontFiles`, and `sGlyphRanges` in the `Language` section if your language needs custom display text or font coverage.

```ini
[Language]
sName = Polish
sFontFiles = NotoSans-Regular.ttf, MyPolishFont.ttf
sGlyphRanges = default, cyrillic
```

`sName` controls how the language appears in the settings menu. `sFontFiles` is a comma-separated fallback order resolved from `Data/Interface/ESPExplorerAE/fonts` first, then `dist/fonts`. `sGlyphRanges` optionally adds full ImGui preset ranges such as `default`, `cyrillic`, `japanese`, `chinese`, `chinese-full`, `korean`, `thai`, or `vietnamese`.

## Installation

1. Install the requirements: F4SE and Address Library.
2. Drop the mod into your Fallout 4 game folder or install with a mod manager of your choice.
3. Launch the game through F4SE.

The default toggle key is `Insert`. You can rebind it at any time. The config file is created automatically next to the plugin in the `F4SE/Plugins` folder.

## Compatibility

This mod may clash with similar explorer/item menu mods. This mod will also likely clash with mods that also hook into mouse input, keyboard input, or similar systems. The following mods may work for your setup, but have been reported to cause issues:

- [P71 In Game Shop Mod Explorer](https://www.nexusmods.com/fallout4/mods/56922)
- [Fast AddItem Menu](https://www.nexusmods.com/fallout4/mods/99301)

Compatibility notes:

- Made for Fallout 4 Anniversary Edition runtime `1.11.191.0`.
- Requires F4SE.
- Requires Address Library.
- Should work alongside most mods, but other input-hook or in-game menu mods may conflict.
- Designed to be safe to install or uninstall mid-save.

## Bug Reports

If something breaks, keep the report short and useful.

- What happened.
- How to reproduce it.
- Your mod list or anything unusual that might matter.
- A screenshot or log if you have one.

Bug reports with clear steps are much easier to fix than reports that only say it does not work.

## Contributing

All developers are welcome. Small, focused pull requests are much easier to review and merge than large grab-bag changes.

Please put real care into changes before opening a PR: read the surrounding code, follow the existing style, build locally with `xmake`, and update localization files for any user-facing text. Do not submit blindly vibe-coded PRs. AI assistance is fine, but contributors are responsible for understanding, testing, and explaining the code they submit.

For the full contributor guide, see [contributing.md](contributing.md).

## Build From Source

Run the production build from the workspace root:

```powershell
xmake f -m release -a x64
xmake
```

This writes the production DLL to `build/windows/x64/release/ESPExplorerAE.dll`.

If you want a clean production rebuild first, run:

```powershell
xmake clean
xmake f -m release -a x64
xmake
```

To package the current release or release-debug DLL with the shipped fonts, languages, and themes, run:

```powershell
./Scripts/package_dist.ps1
```

That creates the staging layout under `dist/package/Data/...` and, when 7-Zip or tar is available, also creates a versioned archive such as `dist/package/ESP Explorer AE 1-4-2.7z`.

## Credits

- F4SE team.
- CommonLibF4 / libxse contributors.
- ocornut for ImGui.
- brofield for SimpleIni.
- DeisDev for ESP Explorer AE.

## More Mods

- [Oblivion Remastered mod](https://www.nexusmods.com/oblivionremastered/mods/788)
- [Fallout 4 mod](https://www.nexusmods.com/fallout4/mods/82077)

## Support

[Buy Me a Coffee](https://buymeacoffee.com/deisdev)

## License

ESP Explorer AE is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
