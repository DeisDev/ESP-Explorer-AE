# Changelog

Notable project changes are recorded here using
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This log begins with the adoption of these conventions. Earlier releases have
not yet been reconstructed from verified release records.

## [Unreleased]

Target version: **1.6.0**.

### Added

- Bulk quantity increments, clear controls, and item/ammo totals in Add Item.
- Saved options for including weapon ammo and its default quantity in Add Item.
- Log search with matching-line copying and a jump-to-latest button.
- A project changelog and Semantic Versioning policy, including compatibility
  criteria and release checks for plugin metadata and packaged artifacts.
- Optional bounded performance profiling for data capture, UI interactions, and
  lifecycle transitions.
- Bundled font assets and redistribution notices, with matching debug symbols
  retained in build output.

### Changed

- Add Item quantities start at zero, and quantity shortcuts add to the current
  amount. Pressing +100 twice selects 200; zero-quantity entries are skipped.
- Simplified the Nexus description and corrected outdated feature and
  compatibility claims.
- Menu navigation, header, and close controls use bundled Lucide icons that scale
  with the interface and follow the selected theme.
- The runtime check accepts Steam Fallout 4 1.11.191, 1.11.221, and 1.11.240,
  with F4SE 0.7.7, 0.7.8, and 0.7.9 respectively and the matching Address Library.
  Other executable versions remain excluded.
- Redesigned the menu with sidebar navigation, a custom header, restrained
  accents, and softer controls. Modern Charcoal is an optional shipped theme.
- Separated status information from footer actions and removed the inactive
  footer undo button; action results remain available in Action History.
- Separated engine readers, application services, and UI presentation. Catalog,
  details, inventory, and player status use detached owning data; browser and
  popup state have explicit owners, and redundant caches were removed.
- Favorites now use plugin-relative identities. Legacy entries require review,
  with configuration backups before migration; unresolved favorites are retained.

### Fixed

- Search clearing keeps the field ready for typing, and Plugin Browser uses the
  same Steam keyboard integration as other browsers.
- Inventory search no longer pushes the equipped filter and refresh control
  beyond the available width. Add Item keeps its totals and buttons below a
  scrollable list.
- Prevented inventory capture from dereferencing absent item-modification buffers
  and corrected weapon/armor instance casts to use the game's type information.
- Restored Default Green for new installations and theme resets. Modern Charcoal
  now loads from its own editable theme INI; existing saved colors are preserved.
- Language files, fonts, and themes load from the installed mod directories;
  missing files no longer cause a search in the source checkout's `dist` folder.
- Corrected stale catalog/filter results and range selection across clipped rows.
- Bound Steam keyboard results to the requesting field and rejected stale action
  requests after game-session changes. Inventory actions revalidate stack targets.
- Preserved pending settings after write failures and added rollback of partial
  hook/renderer initialization plus coordinated shutdown cleanup.
- Distinguished dispatched actions from verified changes. Runtime diagnostics
  retain unknown override counts when evidence is absent.

### Removed

- Removed the remaining disabled Undo control from Action History and its help
  text and translations. History entries use the full available width.
