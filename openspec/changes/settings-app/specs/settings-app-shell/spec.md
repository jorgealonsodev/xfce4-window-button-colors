# Settings App Shell Specification

## Purpose

A standalone GTK 3 binary under `src/settings/`, reachable regardless of the
module's enabled state, registered in the XFCE Settings Manager, packaged
alongside the existing `.deb`, and built without introducing any GTK,
xfconf, or libwnck symbol into `liblc-core.a`.

## Requirements

### Requirement: Standalone Reachability

The settings binary MUST launch and present its window successfully whether
or not `xfce4-window-button-colors` is currently present in `/Gtk/Modules`,
and MUST NOT depend on the module `.so` being loaded into any process.

#### Scenario: Launch while module is disabled (manual)

- GIVEN the module is not listed in `/Gtk/Modules`
- WHEN the user launches the settings binary
- THEN its window opens and is usable

#### Scenario: Launch while module is enabled (manual)

- GIVEN the module is enabled and `xfce4-panel` is running
- WHEN the user launches the settings binary
- THEN its window opens without requiring a panel restart

### Requirement: XFCE Settings Manager Registration

The package MUST ship a `.desktop` entry so the binary appears inside the
XFCE Settings Manager, using the categories XFCE's own settings dialogs use
(`X-XFCE-SettingsDialog`, `OnlyShowIn=XFCE`).

#### Scenario: Entry visible after install (manual)

- GIVEN the package is installed
- WHEN the user opens the XFCE Settings Manager
- THEN an entry for this application is listed with a name, icon, and
  category consistent with other XFCE settings dialogs

### Requirement: Packaging Completeness

The build MUST install the new binary, translatable strings under `po/`,
and declare `libxfconf-0-dev` as a `debian/control` build dependency,
resolving the existing contradiction where `openspec/config.yaml` already
claimed it while `debian/control` did not.

#### Scenario: Build and package succeed (headless-testable)

- GIVEN a clean build tree
- WHEN `meson test -C build` runs
- THEN it passes, including compilation of the new binary

#### Scenario: Package is lintian-clean (manual/tooling)

- GIVEN `dpkg-buildpackage -us -uc -b` has produced a `.deb`
- WHEN `lintian` inspects it
- THEN it reports no errors

### Requirement: Core Purity Boundary Preserved

`src/core/` MUST remain free of `gtk_`, `xfconf_`, and `wnck_` symbols after
this change; any GTK, xfconf, or libwnck code introduced by this capability
MUST live outside `src/core/`.

#### Scenario: Symbol scan stays clean (headless-testable)

- GIVEN `liblc-core.a` built with the new `lc-autoload` core module included
- WHEN `nm -u` scans it for `gtk_`, `xfconf_`, `wnck_` symbols
- THEN zero such symbols are found

### Requirement: `module.c` Untouched

This capability MUST NOT modify `src/glue/module.c`.

#### Scenario: No changes to the incident-hardened entry point (manual/review)

- GIVEN the complete diff for this change
- WHEN `src/glue/module.c` is inspected
- THEN it shows zero changes
