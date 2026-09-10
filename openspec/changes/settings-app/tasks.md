# Tasks: Settings App — GUI enable/disable and stored-colour management

Toolchain verified: meson, ninja, gcc, pkg-config, gettext; `libxfconf-0` 4.18.1
present; `libwnck-3.0` already a dependency. `debian/control` `Build-Depends`
has 8 entries, all used — it is already clean; the stale
`libxfce4panel-2.0-dev`/`libxfce4util-dev` claims live only in
`openspec/config.yaml:30`. Both CI workflows already list `libxfconf-0-dev`
in their apt install steps, so no CI edit is required there — only verified.

## Review Workload Forecast

**Neither the proposal's three PRs nor the design's four survive contact
with the actual work. This plan uses eight.** D7 does add core work to
slice 1 as the design says, but the design's own fix (moving `lc-restart`
out) only addresses part of the problem: `lc-autoload` itself — the D1
shape-table orchestration alone, independent of `lc-restart` — already
exceeds the 400-line budget on its own, and a second, previously unplanned
gap surfaced while reading the specs: two `stored-color-management`
scenarios are marked `(headless-testable)` but the design assigns them no
unit test and calls wnck titles "inherently manual." Closing that gap needs
one additive `src/core/lc-winlist.{c,h}` pure module the design does not
list — see Key Learnings.

| Field | Value |
|-------|-------|
| Estimated changed lines | ~2,630 budget-counted (authored, +~80 i18n catalog lines excluded per project convention) |
| 400-line budget risk | High — 8 of 8 slices; 2 likely need `size:exception` |
| Chained PRs recommended | Yes — 8 slices |
| Suggested split | PR 1 → 2 → 3 → … → 8 (feature-branch-chain) |
| Delivery strategy | auto-chain |
| Chain strategy | feature-branch-chain |

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Why 8, with the line math

Calibration used: `lc-winstore.h` is measured ~60% comment (176 lines, ~105
comment); `lc-winstore.c`'s parser is 722 lines for an 11-function API.
Slice 1 of the prior programme measured 1025 against an estimated ~300
(3.4×); Slice 3a there measured ~1388 against ~380, with the miss entirely
in implementation (712 impl vs 622 test lines, a 0.87 ratio — *below* the
1.4 test:impl anchor), because "the record parser itself was far costlier
than modelled." `lc-autoload`'s D1 shape table is structurally the same
kind of multi-branch classify-and-transform engine, so it is weighted the
same way here, not optimistically.

| Slice | Contains | Branch | Parent | Est. lines | Fits 400? | Focused test | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|---|---|---|
| 1 | `lc-restart` + `lc_winstore_xids()` (D4, D7) + tests | `feat/settings-app-01-restart-xids` | `feat/settings-app` | ~260 | Yes | `meson test -C build restart winstore` | N/A — pure `src/core`, no display | Revert `lc-restart.{c,h}`, the `lc_winstore_xids()` addition, `tests/test-restart.c`; nothing downstream depends on it yet |
| 2 | `lc-autoload` pure transforms (`normalize`/`join`/`is_enabled`/`add`/`remove`) + tests | `feat/settings-app-02-autoload-transforms` | 1 | ~460 | No — smallest irreducible cut; `size:exception` if apply confirms | `meson test -C build autoload` | N/A — pure, no display | Revert `lc-autoload.{c,h}` transform bodies + `tests/test-autoload.c`; blocks slice 3 |
| 3 | `lc-autoload` orchestration: vtable + `lc_autoload_set_enabled()` + full D1 table + tests | `feat/settings-app-03-autoload-shape-policy` | 2 | ~580 | No — one cohesive state machine, same precedent as the prior programme's accepted Slice-1 exception | `meson test -C build autoload` | N/A — fake-backend only | Revert the orchestration additions in the same files; blocks slice 4 |
| 4 | Shell + packaging: `main.c`, `lc-autoload-xfconf.c`, meson target, `.desktop`, `debian/control`, `config.yaml`, `POTFILES.in`, version bump | `feat/settings-app-04-shell-packaging` | 3 | ~310 | Yes | `meson compile -C build` | Launch the built binary once, no toggle wired yet | Revert `src/settings/` skeleton + packaging diffs; blocks slice 5 |
| 5 | Toggle + restart UI (D4, D5) | `feat/settings-app-05-toggle-restart-ui` | 4 | ~280 | Yes | `meson compile -C build` (no unit test — GTK glue) | Manual: toggle against a real `xsettings` channel, confirm/cancel restart | Revert `lc-settings-ui.{c,h}` toggle/restart code; blocks slice 7's UI extension |
| 6 | `src/core/lc-winlist.{c,h}` — pure join, new (closes the spec/design gap) + tests | `feat/settings-app-06-winlist-core` | 5 | ~280 | Yes | `meson test -C build winlist` | N/A — pure, synthetic snapshot, no wnck | Revert `src/core/lc-winlist.{c,h}` + `tests/test-winlist.c`; blocks slice 7 |
| 7 | `src/settings/lc-winlist.{c,h}` (D2/D3) + colour-list UI rows/removal/cleanup | `feat/settings-app-07-winlist-glue-ui` | 6 | ~320 | Yes | `meson compile -C build` (no unit test — wnck glue) | Manual: self-XID probe on a real WM, third-party survival end-to-end | Revert `src/settings/lc-winlist.{c,h}` + the UI extension; blocks slice 8's checklist content only |
| 8 | i18n + docs: `po/`, `README.md`, `docs/manual-verification.md` | `feat/settings-app-08-i18n-docs` | 7 | ~140 (+~80 catalog, excluded) | Yes | `meson test -C build` (catalog check) | Manual: switch session locale, read the checklist | Revert `po/`, `README.md`, `docs/manual-verification.md`; independent of everything else |

Program total (budget-counted): 260+460+580+310+280+280+320+140 ≈ **2,630**,
+~80 excluded `es.po` catalog lines. Slices 2 and 3 are flagged as likely
`size:exception` candidates rather than force-split further, matching the
prior programme's own precedent: split only when the halves are
independently buildable/testable, and the D1 orchestration is one cohesive
function that does not split cleanly.

## Phase 1 — Slice 1: `lc-restart` + `lc_winstore_xids()`

- [x] 1.1 RED: `tests/test-restart.c` — exactly 3 elements, `NULL`-terminated, `[0] == "xfce4-panel"`, `[1] == "-r"`, no element is `sh`/`-c` or a shell metacharacter.
- [x] 1.2 GREEN: `src/core/lc-restart.{c,h}` — `lc_restart_panel_argv()` (D4).
- [x] 1.3 RED: extend `tests/test-winstore.c` — `lc_winstore_xids()` on an empty store returns an empty array with `*out_n == 0`; order is stable; the result feeds `lc_winstore_reconcile()` unchanged (D7).
- [x] 1.4 GREEN: `src/core/lc-winstore.{c,h}` — additive `lc_winstore_xids()` only, no other change to the file.
- [x] 1.5 Wire `tests/meson.build`: add the `test-restart` executable and `test()`.
- [x] 1.6 Verify: `meson test -C build`; `nm -u build/liblc-core.a` still zero `gtk_`/`xfconf_`/`wnck_`.

## Phase 2 — Slice 2: `lc-autoload` pure transforms

- [x] 2.1 RED: `tests/test-autoload.c` — `lc_autoload_add` on an empty list (module-autoload-toggle "Add to an empty list").
- [x] 2.2 RED: same file — `lc_autoload_remove` preserves every other entry, unchanged, in order ("Remove preserves other entries").
- [x] 2.3 RED: same file — `lc_autoload_remove` when absent is a no-op ("Remove when absent is a no-op").
- [x] 2.4 RED: same file — `lc_autoload_normalize` splits `:`-joined scalars, trims, drops empties; `lc_autoload_join` round-trips; `lc_autoload_is_enabled` is exact-match only (`"ours-extra"` ≠ `"ours"`).
- [x] 2.5 GREEN: `src/core/lc-autoload.{c,h}` — `normalize`/`join`/`is_enabled`/`add`/`remove` only. Declare (do not yet define) `LcAutoloadBackend` and `lc_autoload_set_enabled()` for Phase 3.
- [x] 2.6 Wire `tests/meson.build`: add the `test-autoload` executable and `test()`.
- [x] 2.7 Verify: `meson test -C build`; `nm -u build/liblc-core.a` still clean.

## Phase 3 — Slice 3: `lc-autoload` orchestration and the D1 shape table

- [ ] 3.1 RED: extend `tests/test-autoload.c` — a fake `LcAutoloadBackend` recording every `write` call, its shape, and its list.
- [ ] 3.2 RED: same — key absent, enable ⇒ key created holding exactly our module ("Key absent, enable").
- [ ] 3.3 RED: same — scalar holding only ours, enable is idempotent, no duplication ("...enable is idempotent").
- [ ] 3.4 RED: same — scalar holding a different module, enable preserves it byte-for-byte, no duplicate ("...enable preserves it").
- [ ] 3.5 RED: same — array with third-party modules, enable preserves them, ours appears exactly once ("...enable preserves them").
- [ ] 3.6 RED: same — enabling twice returns `LC_AUTOLOAD_NO_CHANGE` and `write` is never called ("Enabling twice is idempotent").
- [ ] 3.7 RED: same — scalar holding only ours, disable clears the key entirely ("...disable clears it").
- [ ] 3.8 RED: same — array with third-party modules, disable removes only ours, others byte-for-byte unchanged ("...disable removes only ours").
- [ ] 3.9 RED: same — disabling when not present is a no-op, `write` never called ("Disabling when we are not present is a no-op").
- [ ] 3.10 RED: same — `LC_AUTOLOAD_UNSUPPORTED_SHAPE` on an unrecognised `GType`, `write` never called; also: empty array, array with `NULL` elements, `"::"`, whitespace-only element (threat matrix: untrusted `/Gtk/Modules` input).
- [ ] 3.11 RED: same — `read` returning `FALSE` ⇒ `LC_AUTOLOAD_READ_FAILED`, no write; `write` returning `FALSE` ⇒ `LC_AUTOLOAD_WRITE_FAILED`.
- [ ] 3.12 GREEN: `src/core/lc-autoload.{c,h}` — `LcAutoloadShape`/`LcAutoloadResult` enums, `LcAutoloadBackend`, `lc_autoload_set_enabled()` implementing D1's full table.
- [ ] 3.13 Verify: `meson test -C build`; `nm -u build/liblc-core.a` still clean; confirm every D1-table test asserts a third-party entry survives.

## Phase 4 — Slice 4: Shell and packaging

- [ ] 4.1 Create `src/settings/main.c` — `GtkApplication`, window bootstrap, gettext domain binding.
- [ ] 4.2 Create `src/settings/lc-autoload-xfconf.c` — the sole `LcAutoloadBackend` implementation against the `xsettings` channel's `/Gtk/Modules` key.
- [ ] 4.3 Edit `meson.build` — `xfconf_dep = dependency('libxfconf-0')`; `executable('xfce4-window-button-colors-settings', ..., dependencies: [lc_core_dep, gtk3_dep, wnck_dep, xfconf_dep])`; `i18n.merge_file()` for the `.desktop`.
- [ ] 4.4 Create `data/xfce4-window-button-colors-settings.desktop.in` — `Categories=XFCE;GTK;Settings;DesktopSettings;X-XFCE-SettingsDialog;`, `OnlyShowIn=XFCE;`.
- [ ] 4.5 Edit `debian/control` — add `libxfconf-0-dev` to `Build-Depends` (D6). No `Depends` edit; runtime `libxfconf-0-3` arrives via `${shlibs:Depends}`.
- [ ] 4.6 Edit `openspec/config.yaml` (line 30 area) — correct `toolchain_present`, dropping the stale `libxfce4panel-2.0-dev`/`libxfce4util-dev` entries `meson.build` does not reference (D6).
- [ ] 4.7 Verify only, no edit needed — `.github/workflows/ci.yml` (read-only) and `.github/workflows/release.yml` (read-only) already list `libxfconf-0-dev` in their apt install steps.
- [ ] 4.8 Edit `po/POTFILES.in` — add `src/settings/main.c`, `src/settings/lc-settings-ui.c`, plus a `[type: gettext/desktop]` line for the `.desktop.in`.
- [ ] 4.9 Bump version to `0.2.0` in `meson.build` (`project()` `version:`) and add a matching new entry to `debian/changelog`, same format as the existing `0.1.0` entry.
- [ ] 4.10 Verify: `meson test -C build`; `meson compile -C build` builds the new binary; `nm -u build/liblc-core.a` still clean — only `src/settings/` links xfconf/gtk/wnck.

## Phase 5 — Slice 5: Toggle and restart UI

- [ ] 5.1 Create `src/settings/lc-settings-ui.{c,h}` — toggle row bound to `lc_autoload_is_enabled()`/`lc_autoload_set_enabled()` via the xfconf backend (D1).
- [ ] 5.2 Wire `XfconfChannel::property-changed` on `xsettings`, filtered to `/Gtk/Modules`, handler blocked while reflecting external state (D5) ("Toggle completes without restarting").
- [ ] 5.3 Add the "Restart panel" action: confirmation dialog naming the `README.md:113` TTY recovery, `g_spawn_async()` with `lc_restart_panel_argv()`, `G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD` (D4) ("Restart is cancellable", "Confirmed restart applies the change").
- [ ] 5.4 Add non-modal error surfacing on spawn failure: the `GError` message plus the literal command in a selectable label, no automatic retry (D4).
- [ ] 5.5 Add the session-local "configuration changed since this window opened" hint; the restart action itself stays enabled regardless (D5).

## Phase 6 — Slice 6: `lc-winlist` pure core (new — closes a spec/design gap)

- [ ] 6.1 RED: `tests/test-winlist.c` — given stored XIDs and a synthetic (non-wnck) snapshot of `{xid, title}` pairs with `snapshot_ok == TRUE`, the built row list contains exactly the live entries with their current titles and none for a closed window (stored-color-management "Orphaned entry never appears as a row", "Row shows the current title").
- [ ] 6.2 RED: same — `snapshot_ok == FALSE` (unobtainable or obtained-empty) ⇒ no entry is marked orphaned and none is presented as live; the result signals an "unavailable" state, never an empty list (stored-color-management "List never marks entries as orphaned under an ambiguous snapshot").
- [ ] 6.3 GREEN: `src/core/lc-winlist.{c,h}` — pure join of `lc_winstore_xids()` output against a caller-supplied snapshot struct, reusing `lc_winstore_reconcile()`'s `snapshot_ok` semantics; no wnck symbol.
- [ ] 6.4 Wire `tests/meson.build`: add the `test-winlist` executable and `test()`, `lc_core_dep` only.
- [ ] 6.5 Verify: `meson test -C build`; `nm -u build/liblc-core.a` still clean.

## Phase 7 — Slice 7: `lc-winlist` glue and the colour-list UI

- [ ] 7.1 Create `src/settings/lc-winlist.{c,h}` — `GtkWidget::map`-gated `wnck_screen_force_update()`, self-XID completeness probe (D2), `snapshot_ok` determination, calling into `src/core/lc-winlist.c`'s pure join (D3).
- [ ] 7.2 Extend `src/settings/lc-settings-ui.c` — colour-list rows, single-entry removal (`lc_winstore_unset()` + `lc_winstore_save()`), bulk orphan cleanup calling `lc_winstore_reconcile()` unchanged with save-iff-dropped-greater-than-zero (stored-color-management "Removing one entry leaves the rest intact", "Cleanup removes only confirmed orphans", "Cleanup is idempotent on a stable, valid snapshot").
- [ ] 7.3 Verify: `meson test -C build`; `nm -u build/liblc-core.a` still clean; `src/glue/module.c` (read-only) shows zero changes (settings-app-shell "module.c Untouched").

## Phase 8 — Slice 8: i18n and docs

- [ ] 8.1 Mark every new user-facing string (toggle label, restart confirm/error dialogs, colour-list labels, cleanup report) for gettext extraction; regenerate `po/xfce4-window-button-colors.pot`.
- [ ] 8.2 Add the new `msgid` entries to `po/es.po`.
- [ ] 8.3 Edit `README.md` — make the settings application the primary activation route; demote the `xfconf-query`/`xfce4-panel -r` commands (`README.md:20-24`) to an alternative section.
- [ ] 8.4 Fix `README.md:72` — the documented `xfconf-query -c xsettings -p /Gtk/Modules -r` reset command deletes a third-party module's autoload when the key lists others (D1); replace it with the settings app's disable action as the ordinary path. `README.md:113`'s identical command stays unchanged there — that one is the correct TTY recovery instruction.
- [ ] 8.5 Create `docs/manual-verification.md` — consolidate the manual scenarios from Phases 4, 5, 7 and settings-app-shell's manual scenarios (launch while disabled/enabled, Settings Manager entry visible, lintian-clean) into one live checklist.
- [ ] 8.6 Verify: `meson test -C build` including the `es_ES.UTF-8` re-run; `msgfmt --statistics` shows no missing entries in `es.po` against the `.pot`; `dpkg-buildpackage -us -uc -b && lintian` clean.

## Key Learnings

1. `lc-autoload`'s D1 shape table is a multi-branch classify-and-transform engine structurally similar to `lc-winstore`'s record parser, so the prior programme's "the record parser was costlier than modelled" lesson applies directly and was weighted accordingly rather than optimistically.
2. `stored-color-management`'s spec marks "Orphaned entry never appears as a row" and "Row shows the current title" `(headless-testable)`, but the design assigns no unit test for either and calls wnck titles "inherently manual" — resolved here by adding an unplanned `src/core/lc-winlist.{c,h}` pure module so those two scenarios are genuinely unit-testable against a synthetic snapshot.
3. Both CI workflows' apt install steps already list `libxfconf-0-dev`; only `debian/control`'s `Build-Depends` and `openspec/config.yaml`'s `toolchain_present` actually needed the fix, so the corresponding task was written as verify-only rather than a redundant edit.
4. The proposal's three PRs and the design's four both undercount: `lc-autoload`'s orchestration alone (D1 table plus its fake-backend test matrix) exceeds the 400-line budget independent of `lc-restart`, requiring the core module itself to split into transforms (Phase 2) and orchestration (Phase 3).
5. `README.md:72` and `README.md:113` carry the identical disable command, but only line 72 is wrong under this design — line 113 is legitimately the TTY recovery instruction and must stay untouched.
