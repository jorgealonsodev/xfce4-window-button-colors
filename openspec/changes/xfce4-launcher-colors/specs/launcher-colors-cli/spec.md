# Launcher Colors CLI Specification

## Purpose

Defines the `xfce4-launcher-colors` command-line interface for scripted
color management and autoload control, independent of the graphical context
menu.

## Requirements

### Requirement: set Command (RF-10)

The system MUST provide `xfce4-launcher-colors set <id> <#rrggbbaa>` to
assign a color to a launcher id, writing the result to `colors.css` using the
same schema as the GUI path — the id+fingerprint marker, and the base/`:hover`/
`:active` rule triplet defined by the persistence capability's Generated Rule
Shape requirement. To compute the fingerprint, `set` MUST look up the
launcher's current desktop-file list from the xfconf `xfce4-panel` channel,
and MUST reject the command with a non-zero exit status and an error message,
without writing anything, when `<id>` does not correspond to a launcher
currently present in that channel.
(Previously: this requirement described writing a bare id+color rule; it now
writes the same fingerprint marker and three-rule shape the GUI path writes,
so a CLI-set color is neither silently discarded as unverifiable nor left
without hover/pressed feedback on the next module load.)

#### Scenario: set writes a valid entry with its marker and rule triplet

- GIVEN a valid launcher id that exists in the xfconf `xfce4-panel` channel, and an 8-digit hex RGBA value
- WHEN `set` is invoked with them
- THEN `colors.css` contains a `/* id=<id> fp=<fingerprint> */` marker
  immediately followed by the base, `:hover`, and `:active` rules for that id
  under the `#launcher-<id> #launcher-arrow` selector
- Unit-testable under Meson: yes, for the command's underlying write logic
  given a mocked xfconf lookup; the live xfconf read is glue, verified manually.

#### Scenario: set rejects a malformed color value

- GIVEN an argument that is not a valid `#rrggbbaa` hex string
- WHEN `set` is invoked with it
- THEN the command exits with a non-zero status and an error message, and `colors.css` is left unchanged

#### Scenario: set rejects an id that is not a known launcher

- GIVEN an `<id>` that does not correspond to any launcher plugin currently in the xfconf `xfce4-panel` channel
- WHEN `set` is invoked with that id
- THEN the command exits with a non-zero status and an error message, without writing any entry to `colors.css`
- Unit-testable under Meson: yes, for the validation logic given a mocked xfconf lookup returning "not found".

### Requirement: unset and list Commands (RF-10)

The system MUST provide `unset <id>` to remove a launcher's entire stored
color entry — its fingerprint marker and all three generated rules (base,
`:hover`, `:active`) — from `colors.css`, and `list` to print every stored id
together with its base color, treating the `:hover` and `:active` rules as
derived output rather than separate entries.
(Previously: unset/list were specified against a single generated rule per
entry; the generated shape is now a marker plus three rules per entry, so
both commands operate on the full entry rather than one CSS rule.)

#### Scenario: unset removes the entire entry, marker and all three rules

- GIVEN `colors.css` has entries for ids 5 and 9, each a marker plus a base/
  `:hover`/`:active` rule triplet
- WHEN `unset 5` is invoked
- THEN the marker and all three rules for id 5 are removed, and id 9's marker
  and triplet remain fully intact
- Unit-testable under Meson: yes.

#### Scenario: list reports one base color per launcher, not per rule

- GIVEN `colors.css` has two stored entries, each expanded into a marker and three rules
- WHEN `list` is invoked
- THEN exactly two id/color pairs are printed — one per launcher, using each
  entry's base-rule color — never six
- Unit-testable under Meson: yes.

### Requirement: --enable and --disable Flags (RF-6, RF-10)

The system MUST provide `--enable` to add the module to `/Gtk/Modules` and
reload the panel, and `--disable` to remove it, mirroring the GUI autoload
toggle.

#### Scenario: --enable activates the module for the next session

- GIVEN the module is absent from `/Gtk/Modules`
- WHEN `--enable` is invoked
- THEN the xfconf key is updated and the running panel is reloaded to pick up the module
- The underlying key-write logic is unit-testable under Meson; the reload
  effect requires manual verification against a live xfconf/panel.

#### Scenario: --disable deactivates without deleting saved colors

- GIVEN the module is active via `/Gtk/Modules`
- WHEN `--disable` is invoked
- THEN the xfconf key is cleared, the panel reloads without the module, and `colors.css` is left untouched
- Manual verification only: requires a live xfconf/panel.
