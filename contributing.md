# Contributing to ESP Explorer AE

Thanks for wanting to help. All developers are welcome here, whether you are fixing a typo, improving translations, tracking down a crash, or building a new feature.

This is a small repository, so the best contributions are focused, readable, and easy to review. A good pull request should make one clear change, explain why it exists, and show that the author put effort into validating it.

## Before You Start

- Check the current source tree before relying on older plans or comments.
- Open an issue or discussion first for large features, behavior changes, UI reorganizations, hook changes, or anything that may affect compatibility.
- Keep pull requests focused. Separate unrelated cleanup, refactors, and feature work into separate PRs.
- Avoid speculative rewrites. Prefer the existing module boundaries and helper patterns unless there is a concrete reason to change them.

## Pull Request Expectations

- Describe the problem and the fix in plain language.
- Include reproduction steps for bug fixes when possible.
- Include screenshots or short notes for visible UI changes.
- Build locally with `xmake` before submitting.
- Do not submit blindly vibe-coded PRs.

AI-assisted code is allowed, but the contributor is responsible for the result. Please read and understand the code before submitting it, remove hallucinated or unused pieces, and be ready to explain how the change works. PRs that look pasted in without review, validation, or care may be closed even if the idea is good.

## Code Style

- Follow the style of the nearby files.
- Keep changes small and direct.
- Prefer existing helpers for localization, filters, form actions, context menus, and shared UI behavior.
- Keep data collection in data modules and presentation/action behavior in UI modules.
- Avoid hardcoding game data when Fallout 4 or F4SE exposes an API that can provide it.

## Localization

Localization is part of feature completeness.

- Route every user-facing string through the existing localization helpers.
- Add new English keys to `dist/lang/en.ini`.
- Mirror every new key across all shipped language files in `dist/lang`.
- If you cannot provide a real translation, copy the English value rather than leaving the key missing.
- Treat English as the canonical reference.

## Testing

There is no dedicated automated test suite right now. Please validate changes with the tools available:

- Run `xmake` from the repository root.
- For packaging changes, run `xmake package`.
- For UI changes, test in-game when possible and mention what you checked.
- For bug fixes, verify the old behavior fails and the new behavior works when practical.

## Good First Contributions

- Translation fixes.
- Documentation improvements.
- Small UI polish with localization included.
- Narrow bug fixes with clear reproduction notes.
- Diagnostics or logging improvements that help future reports.

## Changes That Need Extra Care

- Hook installation or input interception.
- Font atlas rebuild timing.
- Save-affecting player actions.
- DataManager refresh behavior.
- Large UI reorganizations.
- Anything that assumes only Latin glyphs are needed.

When in doubt, keep the PR smaller and explain the tradeoff. Review is much smoother when the intent is clear and the diff is boring in the best possible way.
