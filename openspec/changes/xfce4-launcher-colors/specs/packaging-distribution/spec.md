# Packaging and Distribution Specification

## Purpose

Defines the installable artifacts, their dependencies, size, and license, so
installation and removal never disturb the base XFCE system outside the
module's own files.

## Requirements

### Requirement: .deb Package Contents and Dependencies (RNF-4, RNF-5)

The system MUST be packaged as a `.deb` for `amd64` and `arm64` declaring
`Depends` limited to `libgtk-3-0, libxfce4panel-2.0-4, libxfce4util7,
libxfconf-0-3, xfce4-panel (>= 4.16)`, and the resulting package MUST be
smaller than 100 KB.

#### Scenario: Package declares only the approved dependency set

- GIVEN the built `.deb` control file
- WHEN its `Depends` field is inspected
- THEN it lists exactly the approved libraries and no GUI toolkit beyond GTK3
- CI-verified via `dpkg-deb -I`, not a Meson unit test.

#### Scenario: Package size stays under budget

- GIVEN a built `.deb` for a release
- WHEN its file size is measured
- THEN it is smaller than 100 KB
- CI-verified via build-artifact inspection.

### Requirement: tar.gz Source Distribution with install.sh

The system MUST provide a `tar.gz` source tarball with a bundled `install.sh`
that detects the host package manager, reports build dependencies, and runs
the Meson build and install, with a `--user` option for a non-root install
path.

#### Scenario: install.sh completes a root install

- GIVEN a supported distribution with build dependencies present
- WHEN `install.sh` runs without `--user`
- THEN it builds and installs the module system-wide via `ninja install`
- Manual/CI-matrix verification only, per distribution.

#### Scenario: install.sh supports a non-root install

- GIVEN the user passes `--user`
- WHEN `install.sh` runs
- THEN it installs into `~/.local` without requiring root privileges
- Manual/CI-matrix verification only.

### Requirement: Minimal Runtime Dependencies (RNF-4)

The system MUST depend only on libraries already present on a standard XFCE
install (GTK3, libxfce4panel, libxfce4util, xfconf) and MUST NOT introduce
Python, Electron, or Qt runtime dependencies.

#### Scenario: Dependency audit finds no disallowed runtime

- GIVEN the final built package
- WHEN its linked libraries and declared dependencies are audited
- THEN none of Python, Electron, or Qt appear
- CI-verified via a dependency audit step.

### Requirement: License Declaration (RNF-9)

The system MUST be licensed under GPL-2.0-or-later, declared in packaging
metadata and source headers.

#### Scenario: License field matches source headers

- GIVEN the `.deb` control file and the source file headers
- WHEN both are inspected
- THEN both declare GPL-2.0-or-later consistently
- CI-verified via a license-lint step.

### Requirement: Cross-Version Compatibility Matrix (RNF-7)

The system MUST function on `xfce4-panel` 4.16, 4.18, and 4.20, and MUST
detect and warn once, without crashing, when running against an older
unsupported version.

#### Scenario: Module warns once on an unsupported panel version

- GIVEN the module loads under an `xfce4-panel` version below 4.16
- WHEN initialization runs its version check
- THEN it emits exactly one warning and disables its own further activity without crashing the panel
- Requires environments not available locally; deferred to the H3 CI matrix.

#### Scenario: Menu API and CSS behavior are consistent across supported versions

- GIVEN the module is loaded under each of 4.16, 4.18, and 4.20
- WHEN a color is applied and a menu item is inserted
- THEN both behave identically across all three versions
- CI-matrix verification only; not reproducible in this environment.
