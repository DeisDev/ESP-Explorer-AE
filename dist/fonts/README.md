# Bundled font assets

These five OFL 1.1 fonts are vendored in the repository. A normal checkout obtains
all required bytes; no network downloader, font conversion tool, system-font
installation or Git LFS setup is required. `manifest.json` pins their sizes,
SHA-256 hashes, versions, family project links and redistribution notices.

The files are the exact baseline binaries already present in the implementation
workspace. Version and copyright information were read from their font name
tables. Their historical download URLs and conversion commands are unknown;
project links are family provenance, not a claim that a current upstream download
has the same bytes. Keep these files unchanged to preserve the existing fonts.
All five are TrueType outlines, including the three regional Noto Sans fonts.

Run `python Scripts/validate_assets.py` before building a package. It rejects
missing/changed fonts or notices, missing localization keys and missing glyphs
in plugin-owned text. The package includes the fonts, manifest and all notices.
The OFL notices apply to fonts independently of the plugin's GPL license.

To intentionally change a font, verify its redistribution terms, update its
notice and manifest entry, run asset validation, and validate appearance and
language switching in the supported game runtime. Do not regenerate manifest
hashes merely to silence a failure. Restore damaged files from the same source
revision instead.

Language metadata selects fonts in order, followed by the existing default
font priority: Share Tech Mono, Noto Sans, Noto Sans JP, Noto Sans SC, Noto Sans KR.
Custom runtime language/font assets remain supported. This manifest validates
the shipped bundle; it does not restrict user-supplied runtime files.
