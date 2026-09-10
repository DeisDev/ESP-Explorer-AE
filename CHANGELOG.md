# Changelog

Notable project changes are recorded here using
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This log begins with the adoption of these conventions. Earlier releases have
not yet been reconstructed from verified release records.

## [Unreleased]

Target version: **1.6.0**. In-game validation remains pending.

### Added

- A project changelog and Semantic Versioning policy, including compatibility
  criteria and release checks for plugin metadata and packaged artifacts.
- Optional bounded performance profiling for data capture, UI interactions, and
  lifecycle transitions.
- Bundled font assets and redistribution notices, with matching debug symbols
  retained in build output.

### Changed

- Separated engine readers, application services, and UI presentation. Catalog,
  details, inventory, and player status use detached owning data; browser and
  popup state have explicit owners, and redundant caches were removed.
- Favorites now use plugin-relative identities. Legacy entries require review,
  with configuration backups before migration; unresolved favorites are retained.

### Fixed

- Corrected stale catalog/filter results and range selection across clipped rows.
- Bound Steam keyboard results to the requesting field and rejected stale action
  requests after game-session changes. Inventory actions revalidate stack targets.
- Preserved pending settings after write failures and added rollback of partial
  hook/renderer initialization plus coordinated shutdown cleanup.
- Distinguished dispatched actions from verified changes and disabled speculative
  undo. Runtime diagnostics retain unknown override counts when evidence is absent.
