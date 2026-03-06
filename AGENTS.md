# AGENTS.md

## Purpose

This repository builds an F4SE plugin for Fallout 4 AE / Next-Gen that renders an in-game ImGui explorer for plugins, forms, player actions, and diagnostics.

Use this file as the working guide for LLM-driven changes. Prefer the current source tree over PLAN.md when they disagree.

## Ground Truth

- Source code is under `src/`.
- Runtime language assets are loaded from `Data/Interface/ESPExplorerAE/lang` when present, otherwise from `dist/lang` during development.
- Runtime font assets are loaded from `Data/Interface/ESPExplorerAE/fonts` when present, otherwise from `dist/fonts` during development.
- The current root does not contain `.clang-format` or `.editorconfig`. Follow surrounding file style instead of assuming planned formatting files exist.

## Build And Packaging

- Build from the workspace root with `xmake`.
- Build output is expected under `build/windows/x64/release/` or `build/windows/x64/releasedbg/`.
- Packaging is handled by `Scripts/package_dist.ps1`.
- Packaging copies `dist/lang/*.ini`, `dist/fonts/*.ttf`, and the newest built DLL into `dist/package/Data/...` and optionally creates `dist/package/ESPExplorerAE.7z`.
- There is no dedicated automated test suite in the current repo. Validation is primarily compile verification plus targeted inspection.

## Runtime Flow

Initialization order matters.

1. `src/main.cpp` loads config first.
2. Logging is initialized from config.
3. Localization is loaded from the configured language code.
4. F4SE messaging is registered.
5. Hooks are installed.
6. DataManager refreshes cached game data.

Important consequence: any feature that depends on localized strings or font coverage should assume config and language are ready before the UI is first drawn.

## Main Modules

### Core

- `src/main.cpp`: plugin entry point, initial boot, F4SE message handling.
- `src/Hooks/`: D3D11 Present hook, WndProc hook, cursor/input state, menu visibility.
- `src/GUI/ImGuiRenderer.cpp`: ImGui context setup, theme application, font rebuild processing.
- `src/GUI/MainWindow.cpp`: top-level window, shared UI state, tab orchestration, modal popups, status bar.

### Data

- `src/Data/DataManager.cpp`: central cache rebuild from `RE::TESDataHandler` and `RE::TESForm::GetAllForms()`.
- `FormCache` holds categorized views used by tabs.
- `PluginInfo` and category counts are produced during refresh and read through shared locks.

### UI Tabs

- `PluginBrowserTab`: plugin tree, grouped records, details panel, recent items, favorites, global search.
- `ItemBrowserTab`: categorized item tables plus derived categories such as keys, books/notes, aid, and components.
- `NPCBrowserTab`, `ObjectBrowserTab`, `SpellPerkBrowserTab`, `CellBrowserTab`: domain-specific form browsers.
- `PlayerTab`: quick player actions and item shortcuts.
- `SettingsTab`: config editing, theme control, language switching, gamepad settings.
- `LogViewerTab`: in-game view of plugin log output.

### Shared Widgets

- `FormTable`: reusable sortable form list with multi-select and context menus.
- `FormDetailsView`: detailed record inspection.
- `ContextMenu`: action routing based on category.
- `ItemGrantPopup`: item/ammo quantity modal.
- `SearchBar`, `RecordFiltersWidget`, `SharedUtils`: shared UI utilities.

### Support Systems

- `src/Config/`: INI-backed settings via SimpleIni.
- `src/Localization/Language.cpp`: localized string loading with English fallback.
- `src/Localization/FontManager.cpp`: language-aware font atlas building and rebuild requests.
- `src/Input/GamepadInput.cpp`: XInput polling and Steam overlay keyboard support.
- `src/Logging/Logger.cpp`: file logging under Documents/My Games/Fallout4/F4SE.

## Localization Rules

Localization is not optional here. Treat it as part of feature completeness.

- Every user-facing label, button, menu item, status text, popup title, popup body, tooltip, and section header must be localizable.
- Prefer the existing per-file helper pattern such as `L(section, key, fallback)` or injected `localize(...)` callbacks.
- Do not introduce new visible English literals directly in ImGui calls unless the string is an internal ID suffix like `##HiddenId`.
- Add new keys to `dist/lang/en.ini` first.
- Mirror every new key across all shipped language files in `dist/lang/`: `de.ini`, `en.ini`, `es.ini`, `fr.ini`, `ja.ini`, `ko.ini`, `ru.ini`, `zh.ini`.
- If a true translation is unavailable during implementation, copy the English string into the non-English file rather than omitting the key. Missing keys silently fall back, which hides localization debt.
- You should still attempt to translate keys on a best-effort basis instead of relying on copying english strings.
- Keep section/key naming consistent with existing usage such as `General`, `Settings`, `Items`, `NPCs`, `Objects`, `Spells`, `Player`, `PluginBrowser`, `FormDetails`, `Logs`.
- When changing language-selection behavior, preserve the current sequence in `SettingsTab`: save config, reload `Language`, then request a font rebuild through `FontManager::RequestLanguageRebuild`.
- When adding a new supported language or script, update `FontManager::BuildOneSize` and ensure the required font file exists in `dist/fonts` and in packaged output.
- Remember that game data names from Bethesda records are not automatically localized by this plugin. Only plugin-owned UI strings come from the language INI files.

## Localization Review Checklist

For any UI-facing change, verify all of the following before considering the work complete.

1. New visible text is routed through localization helpers.
2. Matching keys exist in every shipped language file.
3. The English file remains the canonical reference.
4. No language change flow was broken.
5. Font coverage still makes sense for the affected languages.
6. Keys were translated on an best-effort basis. 

## Codebase Conventions

- Follow existing brace placement, spacing, and namespace structure in nearby files.
- Prefer small helper lambdas and local static caches where the surrounding code already uses them.
- Preserve current module boundaries. Do not move tab logic into unrelated files without a strong reason.
- Keep data enumeration and UI rendering separate. `DataManager` gathers data; tabs and widgets present and act on it.
- Use existing wrapper points before adding new abstractions. This codebase already has reusable patterns for search, filters, context menus, and localized string lookup.
- Avoid speculative cleanup. This repository contains planned documentation that is ahead of the implementation in places.

## Source-Of-Truth Notes For Agents

- Treat `PLAN.md` as helpful architecture intent, not guaranteed implementation truth.
- The real shipped assets are under `dist/`.
- UI behavior is spread between `MainWindow`, tab files, and shared widgets. Search those together before changing UX.
- Favorites, filters, window state, and theme settings are persisted through `Config::Save()`.
- `DataManager::Refresh()` is relatively heavy and should not be called casually from UI code.

## Safe Change Workflow For LLMs

1. Read the relevant source file and adjacent helpers first.
2. If the change affects visible UI, search the language files before editing code.
3. If the change affects language switching or text rendering, inspect both `Language` and `FontManager`.
4. Build with `xmake` after changes.
5. If packaging behavior is relevant, verify `Scripts/package_dist.ps1` still matches the asset layout.

## High-Risk Areas

- Hook installation and input interception in `src/Hooks/`.
- Font atlas rebuild timing in `src/GUI/ImGuiRenderer.cpp` and `src/Localization/FontManager.cpp`.
- Any change that adds visible text without updating `dist/lang/*.ini`.
- Any change that assumes only Latin glyphs are needed.