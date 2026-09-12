# Changelog

Notable project changes are recorded here using
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This log begins with the adoption of these conventions. Earlier releases have
not yet been reconstructed from verified release records.

## [Unreleased]

Target version: **1.6.0**.

### Added

- Advanced filter rule search, bulk enable/disable, undo, plugin scopes when
  adding rules, and a preview of matching loaded records with examples.
- Search, an attention filter, expandable result details, and copying in
  Action History.
- Resizable Plugin Browser panes, selection clearing, and a compact Copy menu
  with EditorID and record-source copying for single and multiple selections.
- Duplicate inventory items, including weapons and armor, with their extra data,
  attachments, legendary effects, custom name, condition, and instance stats.
- Inventory filters for Pip-Boy favorites, legendary items, and quest items;
  sortable stack weight and value totals; and Select Visible/Clear Selection
  controls with selected quantity and weight totals.
- An All tab in Object Browser combining activators, containers, statics, and
  furniture with the existing search, filters, favorites, and placement controls.
- Bulk quantity increments, clear controls, and item/ammo totals in Add Item.
- Saved options for including weapon ammo and its default quantity in Add Item.
- Log search with matching-line copying and a jump-to-latest button.
- Optional bounded performance profiling for data capture, UI interactions, and
  lifecycle transitions.
- Bundled font assets and redistribution notices, with matching debug symbols
  retained in build output.

### Changed

- Advanced Filters and Action History are resizable tool windows that remain
  available across menu pages. Filter rules use a compact, collapsible composer
  and scrolling cards; hidden plugins use a searchable checklist.
- Plugin Browser keeps action controls above the scrolling record details and
  plugin diagnostics. Bulk controls distinguish the selection from the active
  record, with clearer search scope, empty results, and record tooltips.
- Inventory columns can be resized, reordered, and hidden; the default view
  focuses on name, quantity, and stack weight. Bulk actions use a compact menu
  with item quantities in removal/drop confirmations. Instance details and
  actions appear first, with base-record details in an expandable section.
- Add Item quantities start at zero, and quantity shortcuts add to the current
  amount. Pressing +100 twice selects 200; zero-quantity entries are skipped.
- Menu navigation, header, and close controls use bundled Lucide icons that scale
  with the interface and follow the selected theme.
- The runtime check accepts Steam Fallout 4 1.11.191, 1.11.221, and 1.11.240,
  with F4SE 0.7.7, 0.7.8, and 0.7.9 respectively and the matching Address Library.
  Other executable versions remain excluded.
- Redesigned the menu with sidebar navigation, a custom header, restrained
  accents, and softer controls. Modern Charcoal is an optional shipped theme.
- Separated status information from footer actions and removed the inactive
  footer undo button; action results remain available in Action History.
- Favorites now use plugin-relative identities. Legacy entries require review,
  with configuration backups before migration; unresolved favorites are retained.

### Fixed

- The menu recovers missed focus notifications after loading, so opening it no
  longer depends on alt-tabbing. Rendering uses the current screen buffer even
  when the game leaves a different drawing target active.
- Inventory rows stay in place when selecting items or receiving action feedback,
  and inventory and Plugin Browser details keep a steady width as their content changes.
- Popouts stay above the workspace when browsing, fit the current screen, and
  keep modal dialogs above tool windows. Dialogs wait for existing popups to
  close, and long confirmation messages scroll above the action buttons.
- Clearing all advanced filter rules remains cleared after restarting. New
  rules reject invalid regular expressions, plugin scopes stay distinct, and
  searchable keyword and scope lists no longer stop at a fixed result limit.
- Inventory duplication remains available after making a copy. The selected
  instance survives quantity changes and added stacks, and the context menu
  lets you choose between different instances of the same item.
- Plugin Browser record details no longer stay on "Loading record details..."
  at the initial main menu before starting or loading a game.
- Long detail values wrap within the pane, and changing records returns details
  to the top. Bulk giving remains available for eligible records when the active
  record in the selection is deleted.
- Inventory count editing starts at the current quantity, and equip/unequip
  follows the chosen instance when a group contains mixed equipment states.
  Selection controls fit above the table and details, and empty filter results
  explain how to restore the item list.
- Restored the player HUD after closing the explorer or turning off Hide Player
  HUD When Menu Open, including when hiding the HUD removes its menu object.
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
