# Launcher Colors CLI Specification

## Purpose

Defines the `xfce4-launcher-colors` command-line interface for scripted
color management and autoload control, independent of the graphical context
menu.

## Requirements

### Requirement: set Command (RF-10)

The system MUST provide `xfce4-launcher-colors set <id> <#rrggbbaa>` to
assign a color to a launcher id, writing the result to `colors.css` using the
same schema as the GUI path — including the id+fingerprint marker required
by the persistence capability's Desktop-File Fingerprint Binding requirement.
To compute that fingerprint, `set` MUST look up the launcher's current
desktop-file list from the xfconf `xfce4-panel` channel, and MUST reject the
command with a non-zero exit status and an error message, without writing
anything, when `<id>` does not correspond to a launcher currently present in
that channel.
(Previously: this requirement described writing a bare id+color rule; it now
requires the same fingerprint marker the GUI path writes, so a CLI-set color
is not silently discarded as unverifiable on the next module load.)

#### Scenario: set writes a valid entry with its fingerprint marker

- GIVEN a valid launcher id that exists in the xfconf `xfce4-panel` channel, and an 8-digit hex RGBA value
- WHEN `set` is invoked with them
- THEN `colors.css` contains a `/* id=<id> fp=<fingerprint> */` marker
  immediately followed by a rule for that id using the `#launcher-<id> #launcher-arrow`
  selector and the given color
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
- Unit-testable under Meson: yes.

### Requirement: unset and list Commands (RF-10)

The system MUST provide `unset <id>` to remove a launcher's stored color and
`list` to print every stored id and color pair.

#### Scenario: unset removes only the targeted entry

- GIVEN `colors.css` has entries for ids 5 and 9
- WHEN `unset 5` is invoked
- THEN the entry for id 5 is removed and the entry for id 9 remains
- Unit-testable under Meson: yes.

#### Scenario: list prints all stored entries

- GIVEN `colors.css` has two stored entries
- WHEN `list` is invoked
- THEN both id/color pairs are printed, one per line
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
