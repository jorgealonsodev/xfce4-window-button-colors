# Exploration: settings-app — GUI enable/disable and stored-colour management

Investigation only. Every claim below was independently re-verified by the orchestrator against the repository and the installed system, not taken from the agent report.

## Why this change exists

The user installed the v0.1.0 `.deb`, found no way to turn the module on, and said: *"debería haber entonces una aplicación en el menú para elegir si activarlo o no"*.

Two gaps, one cause — the product has no UI of its own:

1. **Activation requires a terminal.** Installing does not enable; enabling means writing the `xsettings` channel's `/Gtk/Modules` key with `xfconf-query`. Asking a desktop user for a shell command to flip a yes/no switch is a design failure, and the README has been compensating for missing UI with documentation.
2. **Stored colours cannot be managed in bulk.** The only path to a colour is right-clicking its window button, which fails completely once that window is closed — the entry lives on in `colors.css` with no UI route to it at all.

## Current state

| Fact | Evidence |
|---|---|
| No xfconf dependency exists in `src/` | `debian/control` `Build-Depends` has **0** mentions of xfconf |
| `openspec/config.yaml` already claims `libxfconf-0-dev` | **1** mention — stale/aspirational, contradicts the build |
| `libxfconf-0` 4.18.1 is available | `pkg-config --modversion libxfconf-0` |
| `LcWinStoreEntry` persists only `{gulong xid; LcColor color;}` | `src/core/lc-winstore.c:20` |
| `lc-autoload` does not exist | no `src/core/lc-autoload.*` |
| XFCE Settings Manager entry needs specific categories | real `xfce4-color-settings.desktop`: `Categories=XFCE;GTK;Settings;DesktopSettings;X-XFCE-SettingsDialog;X-XFCE-…Settings;` plus `OnlyShowIn=XFCE;` |

**`/Gtk/Modules` has no fixed schema type.** It is not a built-in xsettings default, and the command the README currently documents creates it as a **scalar string**. Existing users therefore have a scalar on disk. Enable/disable code must handle absent, scalar and array shapes; assuming array risks dropping another module's entry.

**`src/glue/module.c` must not be touched.** Its header documents three real production incidents, and more to the point it is unreachable while the module is disabled — which is exactly the state this feature has to escape.

**`settings.ini` is reserved but dead**: `lc-paths` resolves it and nothing reads or writes it. Enable state must never be shadowed there; xfconf is the only source GTK actually reads.

## Prior art, already designed and never built

The superseded launcher-era change (`openspec/changes/xfce4-launcher-colors/`, historical record — **not an active change**) specified decision **D13**: pure list transforms `lc_autoload_add/remove/is_enabled` over `gchar**`, with xfconf I/O isolated behind a two-function `LcAutoloadBackend` vtable. The reason was precisely so that idempotent add and "must not drop other modules" are unit-testable against a fake backend. Directly reusable.

## Approaches

| # | Approach | Verdict |
|---|---|---|
| 1 | Standalone GTK settings binary + `.desktop`, xfconf behind a D13 vtable | **Recommended** |
| 2 | Extend the module `.so` with a settings window | Rejected — unreachable while disabled, so it does not solve the complaint, and it puts xfconf and dialog code into incident-hardened `module.c` |
| 3 | CLI `--enable`/`--disable`/`list`/`unset` | Rejected alone — the request was for a menu application, not another terminal command |

## Recommendation

Approach 1: a separate binary under `src/settings/`, reachable regardless of module state, keeping `src/core/` pure and `module.c` untouched.

**Two decisions worth stating rather than designing around:**

- **A closed window's colour row can only ever show a bare X window id.** The schema stores nothing else, so no human-readable label is recoverable once the window is gone. Open windows can get a live title via libwnck, which is safe in an independent process. This is a permanent limitation of the v0.1.0 schema and belongs in the spec as such.
- **The xfconf write and the panel restart stay two separate, explicit, cancellable actions.** Auto-restarting on toggle would reintroduce exactly the TTY-recovery risk the README documents deliberately.

## Risks

- Misdetecting the `/Gtk/Modules` shape could drop a third-party module's entry. Must be a first-class fake-backend test case, using another module's name and not only our own.
- Auto-restarting the panel would defeat the deliberate-and-reversible design intent.
- The `config.yaml` / `debian/control` contradiction should be named in the proposal so the dependency diff does not read as unexplained scope creep.
- Review budget: at least two chained PRs — (1) core `lc-autoload` + xfconf backend + toggle UI, (2) colour list and removal UI + `.desktop` and packaging.

## Testability

Headless and unit-testable: the `lc-autoload` list transforms in full, against a fake backend — idempotent add, remove preserving other entries, and the absent/scalar/array shape handling.

Inherently manual: the xfconf backend itself, the GTK window, Settings Manager placement, and the panel restart.

## Ready for proposal

Yes.
