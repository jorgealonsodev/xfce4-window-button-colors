# Proposal: Settings App — GUI enable/disable and stored-colour management

## Intent

Enabling the module requires a terminal: `xfconf-query` on `/Gtk/Modules`, then `xfce4-panel -r`. UI hosted inside the module is unreachable in exactly the state the user must escape — module off, no right-click menu. Stored colours only grow; pruning `colors.css` means hand-editing a generated file. Success: enable, disable and clean up from the XFCE Settings Manager, with the panel restart still deliberate. Targets v0.2.0.

## Scope

### In Scope
- Standalone GTK 3 binary in `src/settings/`, launchable whatever the module's state.
- `src/core/lc-autoload.{c,h}`: pure `add`/`remove`/`is_enabled` over `gchar**`, xfconf isolated behind a two-function `LcAutoloadBackend` vtable (superseded decision D13).
- `/Gtk/Modules` shape handling — absent, scalar string (today's README creates a scalar), array — never dropping another module's entry.
- Enable/disable toggle; panel restart as a second, explicit, cancellable action.
- Colour list of currently-open windows only, with real libwnck titles; one "remove orphaned entries" action reporting its count.
- `.desktop` entry, meson target, `debian/control` build-dep, `po/` strings.

### Out of Scope
- `src/glue/module.c` — untouched.
- Persisting app class/title in `LcWinStoreEntry` to label closed windows (schema change, separate proposal).
- Per-row orphan display, title-matching rules, automatic panel restart.

## Capabilities

### New Capabilities
- `settings-app-shell`: the binary, its Settings Manager entry, packaging, and the `src/core` purity boundary.
- `module-autoload-toggle`: `/Gtk/Modules` enable/disable semantics, shape detection, idempotency, separate restart action.
- `stored-color-management`: listing live entries with titles, single removal, bulk orphan cleanup.

### Modified Capabilities
- None. `openspec/specs/` is empty; nothing has been archived.

## Approach

Exploration's Approach 1. The hard logic — list transforms, idempotency, "must not drop other modules" — lives in `src/core` and is unit-tested against a fake backend under `meson test -C build`; only the thin xfconf backend, GTK layout and wnck join stay manually verified.

Three chained PRs, not the two exploration suggested: (1) core `lc-autoload` + vtable + tests; (2) `src/settings/` binary, xfconf backend, toggle and restart UI, `.desktop`, meson, packaging; (3) colour list, orphan cleanup, translations. Slice 2 carries the real 400-line budget risk.

`openspec/config.yaml` `toolchain_present` already claims `libxfconf-0-dev`, while `debian/control` `Build-Depends` has no xfconf entry at all. Adding it corrects that contradiction; it is not scope creep.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/settings/` | New | GTK 3 binary, xfconf backend, `.desktop` |
| `src/core/lc-autoload.{c,h}` | New | Pure transforms + backend vtable |
| `src/core/lc-winstore.*`, `lc-paths.*` | Reused | Read/remove entries; unchanged |
| `src/glue/module.c` | Untouched | Incident-hardened; out of scope |
| `meson.build`, `debian/control` | Modified | New target, `libxfconf-0` |
| `openspec/config.yaml`, `po/`, `README.md` | Modified | Toolchain fix, strings, GUI path |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Write drops a third-party `/Gtk/Modules` entry | Med | Fake-backend tests for absent/scalar/array |
| Empty wnck snapshot mislabels live entries as orphans | Med | Reuse `lc_winstore_reconcile` asymmetry: unobtainable-or-empty prunes nothing |
| Panel fails to load after enabling | Low | Never auto-restart; surface the README's TTY recovery in the UI |
| Slice 2 exceeds 400 lines | Med | Split UI from packaging if the tasks forecast confirms |

## Rollback Plan

Revert per slice. The shipped `.so` and the `colors.css` format do not change, so a revert removes only the binary and `.desktop` and leaves v0.1.0 behaviour intact. A user left disabled recovers with the README's existing `xfconf-query -c xsettings -p /Gtk/Modules -r`.

## Dependencies

- `libxfconf-0` 4.18.1 (present); `libxfconf-0-dev` added to `Build-Depends`.
- `libwnck-3.0`, already a dependency.

## Success Criteria

- [ ] Module enables and disables from the Settings Manager with no terminal.
- [ ] Toggling is idempotent and preserves every other `/Gtk/Modules` entry across absent, scalar and array shapes.
- [ ] The panel never restarts without explicit confirmation.
- [ ] Colour list shows real titles; orphan cleanup reports its count.
- [ ] `meson test -C build` green including the `es_ES.UTF-8` re-run; `liblc-core.a` still free of `gtk_`/`xfconf_`/`wnck_`; lintian clean.
