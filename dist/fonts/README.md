# Bundled font assets

Five OFL 1.1 text fonts and the Lucide icon font are vendored in the repository.
A normal checkout obtains all required bytes; no network downloader, font conversion tool, system-font
installation or Git LFS setup is required. `manifest.json` pins their sizes,
SHA-256 hashes, versions, family project links and redistribution notices.

The text fonts are the exact baseline binaries already present in the
implementation workspace. Version and copyright information were read from their font name
tables. Their historical download URLs and conversion commands are unknown;
project links are family provenance, not a claim that a current upstream download
has the same bytes. Keep these files unchanged to preserve the existing fonts.
All five are TrueType outlines, including the three regional Noto Sans fonts.

Run `python Scripts/validate_assets.py` before building a package. It rejects
missing/changed fonts or notices, missing localization keys and missing glyphs
in plugin-owned text. The package includes the fonts, manifest and all notices.
The font notices apply independently of the plugin's GPL license.

`Lucide.ttf` is the unmodified icon font from the official `lucide-static` 1.44.0
package. Its manifest entry records the archive URL, integrity, member path, and
font hash. The bundled `licenses/Lucide-ISC-MIT.txt` preserves both Lucide's ISC
notice and the MIT notice for icons derived from Feather. Named glyphs in
`src/GUI/Icons.h` come from that package's `font/lucide.css`.

Lucide renders the menu navigation, header, and close icons through ImGui's font
atlas. It uses a separate font to avoid collisions with localized or custom text
glyphs, follows the active theme, and rebuilds with the selected font size and
language. It does not need an entry in language `sFontFiles` lists.

To intentionally change a font, verify its redistribution terms, update its
notice and manifest entry, run asset validation, and validate appearance and
language switching in the supported game runtime. Do not regenerate manifest
hashes merely to silence a failure. Restore damaged files from the same source
revision instead.

Language metadata selects fonts in order, followed by the existing default
font priority: Share Tech Mono, Noto Sans, Noto Sans JP, Noto Sans SC, Noto Sans KR.
Custom runtime language/font assets remain supported. This manifest validates
the shipped bundle; it does not restrict user-supplied runtime files.
