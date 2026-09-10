# Contributing to ESP Explorer AE

Contributions are welcome, from translation fixes and documentation improvements to bug fixes, themes, and new features. You do not need to know the whole codebase to help.

Keep each pull request focused on one clear change. Explain what it improves and how you checked it.

## Before You Start

- Check existing issues and pull requests for related work.
- Open an issue first for a large feature, a UI reorganization, or changes to hooks, input handling, or compatibility. A small fix can go straight to a pull request.
- Read the relevant source and nearby helpers before editing. Follow the patterns already used in that area.
- Keep unrelated cleanup and dependency updates in separate pull requests.

## Build Setup

Use Windows x64, xmake 3.0 or newer, and an MSVC toolchain with C++23 support and the Windows SDK. From your checkout:

```powershell
git submodule update --init --recursive
xmake f -m release -a x64
xmake
```

Keep submodules at the revisions pinned by the repository. The release DLL is written to `build/windows/x64/release/ESPExplorerAE.dll`.

For in-game checks, use Steam Fallout 4 **1.11.240**, F4SE **0.7.9**, and the matching Address Library for F4SE Plugins. See the [build instructions](README.md#build) and [packaging instructions](README.md#packaging) for more detail.

## Code Style

- Follow the style of the nearby files.
- Reuse existing helpers for localization, filters, form actions, and shared UI controls.
- Keep engine reads and actions in `src/Game`, coordinated through `src/App` services. UI code displays prepared data and submits requests.
- Keep `src/Core` independent of game, Windows, and ImGui APIs.
- Prefer game metadata and the pinned CommonLibF4 APIs over hardcoded forms, offsets, or copied bindings.
- Preserve existing settings, favorites, and migration behavior when changing persistence.
- Fix compiler warnings; the plugin build treats them as errors.

## Localization

Include localization with any change to visible text.

- Route every user-facing string through the existing localization helpers.
- Use `dist/lang/en.ini` as the canonical reference, and mirror key additions, changes, and removals across every shipped language file in `dist/lang`.
- Preserve placeholders and formatting in translations.
- If a translation is unavailable, use the English value rather than leaving the key missing.
- Check long labels and non-Latin text for clipping and missing glyphs.

## Themes

Theme source files live in `dist/themes`. Packaging places them under `Data/Interface/ESPExplorerAE/themes`, where the plugin loads them in-game.

- Start from an existing theme file and keep the same key structure.
- Use clear lowercase filenames with hyphens, such as `vault-tec.ini`.
- Check readability in dense tables, popups, and disabled controls at different font sizes.
- Check colors with Pip-Boy color sync both enabled and disabled.
- Run `xmake package` and try the installed theme in-game.

## Check Your Change

Choose checks that cover the behavior you changed:

- **Code:** build with `xmake` and exercise the affected behavior in-game.
- **Bug fixes:** reproduce the problem, then repeat the same steps with the fix.
- **UI and input:** check opening and closing the overlay, focus changes, and the affected keyboard or controller controls. Include screenshots for visible changes.
- **Languages, fonts, themes, or packaging:** run `xmake package` and check the installed files. The plugin loads these files from `Data/Interface/ESPExplorerAE`.
- **Documentation:** verify paths, commands, links, and descriptions against the current source.

Take extra care with hook installation, font rebuilds, catalog refreshes, and player actions. Check relevant load transitions and use a separate save when trying actions that change game state.

## Send a Pull Request

- Use a short, descriptive title and explain the problem and resulting behavior in plain language.
- Link the related issue, if there is one, and include reproduction steps for a bug fix.
- List the checks you ran and their results. Include the game and F4SE versions for in-game checks.
- Add a user-facing entry under `Unreleased` in [CHANGELOG.md](CHANGELOG.md) for a notable change.
- Keep commits focused and their messages short and clear.

AI-assisted contributions are welcome. You are responsible for understanding, reviewing, and checking everything you submit, including generated code. Be ready to explain the change and respond to review feedback.
