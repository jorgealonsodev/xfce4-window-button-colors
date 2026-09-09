# Style Settings Specification

## Purpose

Defines persistence and application of the global corner-radius and margin
style settings shared by every colored launcher.

## Requirements

### Requirement: settings.ini Schema (RF-7)

The system MUST persist a corner radius (0–12 px) and an interior margin in
`~/.config/xfce4-launcher-colors/settings.ini`, and MUST apply a documented
default when the file is absent.

#### Scenario: Defaults apply when settings.ini is absent

- GIVEN no `settings.ini` exists
- WHEN the module loads
- THEN it applies its documented default radius and margin values
- Unit-testable under Meson: yes.

#### Scenario: Out-of-range radius is clamped, not rejected

- GIVEN a stored radius value outside 0–12 px
- WHEN the system reads `settings.ini`
- THEN it clamps the value to the nearest bound within 0–12 px rather than failing to load
- Unit-testable under Meson: yes.

### Requirement: Style Settings Dialog (RF-7)

The system MUST provide a settings dialog with a radius slider and a margin
slider that applies changes to all colored launchers globally.

#### Scenario: Changing radius updates all colored launchers

- GIVEN two launchers have assigned colors
- WHEN the user changes the radius in the settings dialog and confirms
- THEN both launchers' backgrounds update to the new radius
- Manual verification only: visual radius rendering requires a live panel.

### Requirement: settings.ini Read/Write Robustness

The system MUST treat a missing or malformed `settings.ini` as "use
defaults" and MUST NOT propagate a parsing failure to the user or the panel.

#### Scenario: Malformed settings.ini falls back to defaults

- GIVEN `settings.ini` contains invalid syntax
- WHEN the module loads
- THEN it applies default radius and margin values and logs at most one warning
- Unit-testable under Meson: yes.
