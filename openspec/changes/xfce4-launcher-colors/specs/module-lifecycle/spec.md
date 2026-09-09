# Module Lifecycle Specification

## Purpose

Defines the GTK module's load/init behavior, the `xfce4-panel`-only process
guard, launcher detection, CSS provider registration, and the user-facing
autoload toggle — the glue connecting the menu and persistence capabilities
to a running panel.

## Requirements

### Requirement: xfce4-panel-Only Process Guard (RNF-6)

The system MUST check `g_get_prgname()` on module init and MUST return
immediately, performing no further initialization, when the running process
is not `xfce4-panel`.

#### Scenario: Module is inert in a non-panel GTK process

- GIVEN the module `.so` is loaded into an arbitrary GTK3 application via `/Gtk/Modules`
- WHEN `g_get_prgname()` does not equal `"xfce4-panel"`
- THEN the module registers no CSS provider, no emission hook, and produces no other side effect
- Unit-testable under Meson: yes, for the guard function given an injected process name.

### Requirement: Silent Failure, Never Crash the Panel (RNF-6)

The system MUST abort any failing initialization or runtime step silently —
using `GError` out-parameters and at most one `g_warning` — and MUST NOT use
`g_error()`, `g_assert()`, or `abort()` on any code path reachable in the
shipped module.

#### Scenario: A failure during load leaves the panel functional

- GIVEN a step of module initialization fails (e.g. CSS parse error, missing config directory)
- WHEN that failure occurs
- THEN `xfce4-panel` keeps running with its native, unmodified behavior
- Manual/integration verification for the end-to-end guarantee; individual
  failure-handling functions are unit-testable in isolation.

#### Scenario: An unexpected widget type does not crash the callback

- GIVEN the map-emission-hook callback receives a widget with no panel-plugin ancestor
- WHEN the callback runs its type checks
- THEN it returns without dereferencing unexpected state and keeps the hook installed
- Unit-testable under Meson: yes, for the filtering logic given mock widget-type inputs.

### Requirement: Launcher Detection via Map Emission Hook (RNF-2)

The system MUST register exactly one global emission hook on
`GtkWidget::map` at module init and MUST perform only cheap filtering (widget
name prefix or panel-plugin ancestry check) inside the callback before any
further work, since the hook fires for every widget mapped in the process.

#### Scenario: Hook discards non-launcher widgets cheaply

- GIVEN the emission hook fires for a widget unrelated to any launcher
- WHEN the callback evaluates it
- THEN it discards the widget after the cheap filter check, without walking the
  full ancestor chain or performing menu insertion
- The filter predicate is unit-testable under Meson given mock widget properties.
- Overall idle-CPU cost (RNF-2) requires manual measurement against a live panel.

#### Scenario: Hook detects a real launcher exactly once

- GIVEN a launcher plugin widget is mapped
- WHEN the emission hook callback runs
- THEN it identifies the widget as a launcher exactly once and triggers menu-item insertion for it
- Manual verification only: requires a real `XfcePanelPlugin` hierarchy.

### Requirement: CSS Provider Registration and Reload

The system MUST register a `GtkCssProvider` loaded from `colors.css` on the
default screen at module init and MUST reload that provider whenever
`colors.css` changes because a color was applied or removed.

#### Scenario: Provider reflects a newly applied color without restart

- GIVEN the CSS provider is registered and active
- WHEN a color is applied to a launcher
- THEN the provider is reloaded and the new rule takes effect immediately
- Reload-triggering logic is unit-testable under Meson in isolation; visible
  effect requires manual verification.

### Requirement: Autoload Toggle via /Gtk/Modules (RF-6)

The system MUST add itself to the xfconf `/Gtk/Modules` key when autoload is
enabled and MUST remove itself from that key when disabled, defaulting to
enabled after installation.

#### Scenario: Disabling autoload restores the native panel on next session

- GIVEN autoload is enabled and the module is active
- WHEN the user disables "Cargar al iniciar sesión" and starts a new session
- THEN the module does not load, the panel shows its native context menu, and
  saved colors remain stored but not visible
- Manual verification only: requires a session restart.

#### Scenario: Re-enabling autoload restores colors

- GIVEN autoload was disabled and colors were previously saved
- WHEN the user re-enables autoload and starts a new session
- THEN the module loads and every previously saved color reappears
- Manual verification only; depends on the persistence capability's session-restore requirement.

### Requirement: Memory and Startup Footprint (RNF-1, RNF-3)

The module's presence MUST add less than 1 MB of resident memory to the
`xfce4-panel` process, MUST complete module load and initial CSS application
in under 10 ms, and MUST create zero additional OS processes.

#### Scenario: No additional process is created

- GIVEN the module is loaded via `/Gtk/Modules`
- WHEN `ps aux` is inspected
- THEN no process attributable to the module appears
- Manual verification only, with `ps`, `smem`, `pmap` against a live panel.
