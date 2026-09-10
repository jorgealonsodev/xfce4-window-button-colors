# Design: Settings App — GUI enable/disable and stored-colour management

## Technical Approach

A standalone GTK 3 executable, `xfce4-window-button-colors-settings`, under `src/settings/`. It is a
separate process from `xfce4-panel`, so it is reachable in exactly the state the user must escape
(module off) and its failures cannot abort the panel.

All decidable logic lands in `src/core/` as pure glib/gio code, unit-tested against a fake backend:
`/Gtk/Modules` list transforms and shape policy (`lc-autoload`), the restart argv vector
(`lc-restart`), and one additive store accessor. xfconf, GTK and libwnck enter only under
`src/settings/`, never `lc_core` — the CI `nm -u` scan on `liblc-core.a` stays green by construction.
`src/glue/module.c` is not opened.

Two of the three incidents recorded in `src/glue/module.c`'s header are load-bearing here: incident 2
(libwnck reentrancy) and incident 3 (silent data loss from an early, empty or partial wnck snapshot).
D2 and D3 below are that hazard's second caller, and are decided, not noted.

## Architecture Decisions

### D1 — `/Gtk/Modules`: preserve the shape found; never convert, never reset a non-empty key

| Read state | Normalized list after transform | Write-back |
|---|---|---|
| absent | `{ours}` | **scalar string** |
| scalar (`G_TYPE_STRING`) | non-empty | **scalar**, `:`-joined |
| scalar | empty | remove key |
| array (`G_TYPE_PTR_ARRAY` of strings) | non-empty | **array** |
| array | empty | remove key |
| present, any other type | — | **never write**; return `LC_AUTOLOAD_UNSUPPORTED_SHAPE` |

Normalization always splits every scalar element on `:` (GTK's own `gtk-modules` separator), trims,
and drops empties, so `"canberra-gtk-module:ours"` is two entries, not one opaque token.

**Choice**: the on-disk shape is a witness carried from `read` to `write` unchanged. The key is only
removed when the resulting list is genuinely empty. An unrecognised value type refuses the write and
surfaces the manual `xfconf-query` command instead.

**Alternatives considered**: always normalize to a string array (uniform, one write path);
always scalar; unconditional `reset` on disable, as `README.md:72` documents for humans.

**Rationale**: the failure mode to design against is dropping a third-party module's autoload. Every
rejected option can cause it. Converting scalar→array re-types a key that other modules already
depend on, against an xfsettingsd type-handling behaviour we have not verified on the target;
array→scalar has the mirror problem. `reset` on a key that still lists another module deletes that
module's autoload outright — which is precisely why the README's disable line is a human recovery
instruction and must not become the app's disable path. Absent→scalar is chosen because the scalar is
the only shape with positive shipped evidence: the README's own `-n -t string -s` command created it
for every existing v0.1.0 user. Without the `:` split, `remove` would silently no-op and `add` would
append a duplicate — both are third-party-entry corruption in disguise.

### D2 — The live/orphaned join is gated on a positively-witnessed complete snapshot

The settings app never derives "orphan" from a snapshot it cannot prove is complete. Sequence:

1. The window is realized and mapped (`GtkWidget::map`), so our own toplevel is in `_NET_CLIENT_LIST`.
2. `wnck_screen_force_update()` — a synchronous X round-trip (see D3 for why this is legal here).
3. Snapshot the XIDs. **Completeness probe**: our own `GdkWindow`'s XID MUST be present in it.
4. `snapshot_ok = (screen != NULL) && (n_live > 0) && (our XID ∈ snapshot)`.

If `snapshot_ok` is FALSE the app **keeps every entry, prunes nothing, lists nothing as orphaned**,
shows a distinct "the window list is unavailable" state — never an empty list, which would read as
"you have no colours" — and disables the cleanup action. Cleanup calls the existing
`lc_winstore_reconcile(store, snapshot_ok, xids, n)` unchanged, and honours its paired rule: no
`lc_winstore_save()` when it returned 0.

**Choice**: an obtained-but-unwitnessed snapshot is treated exactly as an unobtainable one, matching
`lc_winstore_reconcile()`'s documented fail-safe direction.

**Alternatives considered**: trust the first snapshot; wait a fixed delay (the measured ~1.5 s);
subscribe to `WnckScreen::window-closed` as `src/glue/lc-tasklist.c` does.

**Rationale**: incident 3 is exactly "obtained, zero windows" believed. `n_live > 0` alone was
explicitly called insufficient in `lc-winstore.h` — a non-empty snapshot can still be partial, and
`n_live` cannot detect it. The self-XID probe closes that gap with positive per-process evidence the
panel module could never have: our own mapped window must be in a complete list, so its absence
proves incompleteness rather than merely suggesting it. A fixed delay is a magic number that fails on
a slow machine. The `window-closed` signal is the right mechanism for a long-lived panel process but
useless to a short-lived dialog that must show state the moment it opens.

### D3 — `wnck_screen_force_update()` is called here, and only here

**Choice**: the settings process calls `gtk_init()` itself, does its libwnck work from
`GtkWidget::map` (main loop running, `GdkDisplay` guaranteed), calls `force_update()` once per
refresh, and reads titles with `wnck_window_get_name()`.

**Alternatives considered**: reading titles in-process from the module and exporting them; reading
`_NET_WM_NAME` directly via Xlib and skipping libwnck.

**Rationale**: incidents 1 and 2 were both *in-process-with-the-panel* hazards. Incident 1 was a
libwnck call reached synchronously from `gtk_module_init()` before a display existed; here `gtk_init()`
has returned. Incident 2's `reentrancy_guard` is per-`WnckScreen`, and the re-entry came from the
panel's own tasklist plugin already inside `update_client_list()` — our process contains no other
libwnck client, so there is nothing to re-enter. Crucially, the blast radius differs: a libwnck fault
here kills a settings dialog the user can reopen, not the panel. Re-exporting titles from the module
would put new code into `module.c`, which is out of scope by decision; raw Xlib duplicates libwnck for
no gain when libwnck is already a dependency.

### D4 — Restart: an explicit second action, argv vector, no shell

**Choice**: a separate "Restart panel" button, never wired to the toggle. It opens a confirmation
dialog naming the TTY recovery from `README.md:113`, then runs `g_spawn_async()` with the vector from
the pure `lc_restart_panel_argv()` → `{ "xfce4-panel", "-r", NULL }`, flags
`G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD`. On failure: one non-modal error showing the
`GError` message plus the literal command in a selectable label; no automatic retry.

**Alternatives considered**: restart automatically after a successful toggle;
`g_spawn_command_line_async("xfce4-panel -r")`; a D-Bus call to the panel.

**Rationale**: the launcher-era threat-matrix rule ("argv vector, never `g_spawn_command_line_*`,
never a shell string") still applies and applies harder — that row was written for a CLI, and this is
a GUI that will one day carry a configurable command. `g_spawn_command_line_async()` runs a
`GShell`-parsed string and is a standing injection seam for no benefit at a fixed two-token command.
Auto-restart reintroduces the TTY-recovery risk the README documents deliberately. Failure leaves no
partial state, because the xfconf write already committed independently and is idempotent.

### D5 — Module state is read from xfconf and re-read on `property-changed`

**Choice**: enabled ⇔ `lc_autoload_is_enabled(normalized_list, "xfce4-window-button-colors")`. The app
connects `XfconfChannel::property-changed` on `xsettings`, filters `/Gtk/Modules`, re-reads and
updates the switch with the toggle handler blocked (`g_signal_handler_block`) so reflecting external
state never triggers a write. A session-local "configuration changed since this window opened" flag
drives a "restart the panel to apply" hint; the restart action stays enabled regardless.

**Alternatives considered**: probing `/proc/<pgrep xfce4-panel>/maps` for the `.so` (the README
troubleshooting technique) to show truly-loaded state; polling; reading only at startup.

**Rationale**: xfconf is the only source GTK actually reads; `settings.ini` must never shadow it. The
signal makes concurrent `xfconf-query` edits and a second instance of this app converge for free. The
`/proc` probe adds process enumeration, a permissions surface and a fragile parse to answer a question
the always-available restart action already resolves; it is a legitimate later addition, not a
dependency of this change.

### D6 — `debian/control` is the authoritative dependency contract

**Choice**: add `libxfconf-0-dev` to `debian/control` `Build-Depends`, and correct
`openspec/config.yaml` `toolchain_present` to describe the real set — dropping the launcher-era
`libxfce4panel-2.0-dev` and `libxfce4util-dev`, which `meson.build` does not reference. Runtime
`libxfconf-0-3` arrives via `${shlibs:Depends}`; no `Depends` edit. `dependency('libxfconf-0')` is
declared in `meson.build` and reaches only the `src/settings/` executable, never `lc_core`.

**Alternatives considered**: treat `config.yaml` as authoritative and leave `debian/control` alone;
add xfconf to `lc_core_dep` for convenience.

**Rationale**: `dpkg-buildpackage` enforces `debian/control` and CI runs it; `config.yaml`
`toolchain_present` is descriptive metadata no tool validates, so it was aspirational and drifted.
Only one of the two can be the contract, and it is the one that fails the build when wrong. Adding
xfconf to `lc_core_dep` would defeat the `nm -u` scan's entire purpose.

### D7 — `lc-winstore` gains one additive accessor

`lc-winstore.h` exposes `size`, `has` and `get(xid)` but **no way to enumerate**, so the colour list
cannot be built from the public API today. Add one pure accessor:

```c
/* Newly allocated array of every stored xid, in the store's stable order.
 * Sets *out_n. Never NULL; returns an empty array for an empty store. */
gulong *lc_winstore_xids (const LcWinStore *store, gsize *out_n);
```

**Alternatives considered**: a `GList`-returning variant; a `foreach` callback; reaching into the
`GPtrArray` from `src/settings/`.

**Rationale**: purely additive, no behaviour change, matches the existing out-parameter and
`const gulong *`/`gsize` conventions in `lc_winstore_reconcile()`'s signature, and feeds that function
directly. Reaching past the opaque type would break the seam the whole layering exists to protect.
This corrects the proposal's "`lc-winstore.*` reused, unchanged".

## Interfaces

```c
/* src/core/lc-autoload.h — pure glib. Contains no xfconf symbol. */
typedef enum { LC_AUTOLOAD_SHAPE_ABSENT, LC_AUTOLOAD_SHAPE_SCALAR,
               LC_AUTOLOAD_SHAPE_ARRAY,  LC_AUTOLOAD_SHAPE_UNKNOWN } LcAutoloadShape;

typedef enum { LC_AUTOLOAD_OK, LC_AUTOLOAD_NO_CHANGE, LC_AUTOLOAD_READ_FAILED,
               LC_AUTOLOAD_UNSUPPORTED_SHAPE, LC_AUTOLOAD_WRITE_FAILED } LcAutoloadResult;

typedef struct {
  gboolean (*read)  (gpointer user_data, LcAutoloadShape *out_shape, gchar ***out_modules);
  gboolean (*write) (gpointer user_data, LcAutoloadShape shape, const gchar *const *modules);
} LcAutoloadBackend;                                    /* exactly two functions (D13) */

gchar   **lc_autoload_normalize  (const gchar *scalar);                /* split ':', trim, drop empties */
gchar    *lc_autoload_join       (const gchar *const *modules);        /* ':'-joined scalar form */
gboolean  lc_autoload_is_enabled (const gchar *const *modules, const gchar *name);
gchar   **lc_autoload_add        (const gchar *const *modules, const gchar *name);  /* idempotent */
gchar   **lc_autoload_remove     (const gchar *const *modules, const gchar *name);  /* order-preserving */

/* Orchestration over the vtable — still xfconf-free, so the whole policy
 * (D1's table, idempotency, refuse-on-unknown) is testable with a fake. */
LcAutoloadResult lc_autoload_set_enabled (const LcAutoloadBackend *backend, gpointer user_data,
                                          const gchar *name, gboolean enabled);

/* src/core/lc-restart.h */
gchar **lc_restart_panel_argv (void);   /* { "xfce4-panel", "-r", NULL } */
```

`LC_AUTOLOAD_NO_CHANGE` means `write` was **not** called at all — the same "never write when nothing
changed" rule `lc_winstore_reconcile()` documents, applied to xfconf.

## Data Flow

    Toggle ──► lc_autoload_set_enabled ──► backend.read ──► xfconf get  /Gtk/Modules
                        │  (pure: normalize → add/remove → shape policy D1)
                        └────────────────► backend.write ─► xfconf set/reset   [skipped if NO_CHANGE]
                                                                  │
    xfconf property-changed ◄─────────────────────────────────────┘  (D5, handler blocked)

    Restart (separate, confirmed) ──► lc_restart_panel_argv ──► g_spawn_async(vector)

    colors.css ──lc_winstore_load──► store ──lc_winstore_xids──┐
                                                               ├──► join ──► rows (title via wnck)
    map ──► force_update ──► XIDs + self-XID probe (D2) ───────┘
                                     │ snapshot_ok=FALSE ──► keep all, list unavailable, cleanup off
                                     └ snapshot_ok=TRUE  ──► lc_winstore_reconcile ──► save iff n>0

## File Changes

| File | Action | Description |
|---|---|---|
| `src/core/lc-autoload.{c,h}` | Create | Pure transforms, `LcAutoloadBackend`, `lc_autoload_set_enabled` (D1) |
| `src/core/lc-restart.{c,h}` | Create | `lc_restart_panel_argv()` (D4) |
| `src/core/lc-winstore.{c,h}` | Modify | Additive `lc_winstore_xids()` only (D7) |
| `src/settings/main.c` | Create | `GtkApplication`, window, gettext binding |
| `src/settings/lc-settings-ui.{c,h}` | Create | Toggle row, restart action, colour list, cleanup |
| `src/settings/lc-autoload-xfconf.c` | Create | The only `LcAutoloadBackend` implementation |
| `src/core/lc-winlist.{c,h}` | Create | **Amended (D8)** — pure live/orphan join and the `snapshot_ok` truth table, no GTK/wnck |
| `src/settings/lc-winlist-wnck.c` | Create | The only wnck-touching part: snapshot capture and the self-XID completeness probe (D2/D3) |
| `data/xfce4-window-button-colors-settings.desktop.in` | Create | `Categories=XFCE;GTK;Settings;DesktopSettings;X-XFCE-SettingsDialog;`, `OnlyShowIn=XFCE;` |
| `meson.build` | Modify | `xfconf_dep`; `executable(...)` deps `[lc_core_dep, gtk3_dep, wnck_dep, xfconf_dep]`; `i18n.merge_file` for the `.desktop` |
| `tests/meson.build`, `tests/test-autoload.c`, `tests/test-restart.c` | Create/Modify | `lc_core_dep` only |
| `debian/control` | Modify | `libxfconf-0-dev` in `Build-Depends` (D6) |
| `openspec/config.yaml` | Modify | Correct `toolchain_present` (D6) |
| `po/POTFILES.in` | Modify | New `src/settings/*.c` + `[type: gettext/desktop]` line |
| `README.md`, `docs/manual-verification.md` | Modify/Create | GUI path; live checklist |

## Testing Strategy

| Layer | What | How |
|---|---|---|
| Unit — autoload transforms | `:` splitting, trim, empties; idempotent `add`; `remove` preserving order and others; `is_enabled` exact-match (not substring, so `ours-extra` ≠ `ours`) | `tests/test-autoload.c`, `lc_core_dep` only |
| Unit — shape policy (D1) | Every row of D1's table against a fake backend, **each asserting `canberra-gtk-module` survives**: absent→scalar; scalar with a third party; array with a third party; enable when already enabled ⇒ `NO_CHANGE` **and `write` never called**; disable last ⇒ remove; disable one of two ⇒ never remove; `UNKNOWN` ⇒ `write` never called | Fake backend recording every `write` call, its shape and its list |
| Unit — read/write failure | `read` FALSE ⇒ `READ_FAILED`, no write; `write` FALSE ⇒ `WRITE_FAILED` | Same fake |
| Unit — restart argv | Exactly 3 elements, `NULL`-terminated, `[0] == "xfce4-panel"`, `[1] == "-r"`, no element is `sh`/`-c` or contains a shell metacharacter | `tests/test-restart.c` |
| Unit — enumeration (D7) | Empty store ⇒ empty, `*out_n == 0`; order stable; result feeds `lc_winstore_reconcile()` unchanged | `tests/test-winstore.c` |
| Unit — join fail-safe (D2) | The `snapshot_ok` truth table exercised through `lc_winstore_reconcile()`: unobtainable ⇒ 0 dropped; obtained-empty ⇒ 0 dropped; obtained non-empty ⇒ prunes only confirmed absences | `tests/test-winstore.c` — pure, no wnck |
| Build guard | `nm -u build/liblc-core.a` has zero `gtk_`/`xfconf_`/`wnck_` (existing CI job, now covering `lc-autoload.o` and `lc-restart.o`) | GitHub Actions |
| Locale re-run | Full suite under `LC_ALL=es_ES.UTF-8` | Existing CI job |
| Manual (live) | The xfconf backend against a real channel; the self-XID probe on a real WM; Settings Manager placement and icon; titles on a real session; restart actually restarting; **third-party survival end-to-end — add a real second module by hand, toggle ours on and off, confirm the other survives**; recovery from a failed restart | `docs/manual-verification.md` |

Concretely headless-testable: 100 % of D1, D4's vector, D7, and D2's decision table. Inherently
manual: xfconf I/O, GTK layout, `.desktop` placement, wnck titles, the spawn itself.

## Threat Matrix

| Boundary | Applicability | Design response | Planned RED tests |
|---|---|---|---|
| Subprocess — `xfce4-panel -r` | **Applicable** | D4: `g_spawn_async` with an argv vector; never `g_spawn_command_line_*`, never `sh -c` | Vector shape assertions; no shell metacharacter in any element |
| xfconf key mutation — `/Gtk/Modules` | **Applicable** | D1: shape witness preserved; `:`-normalized; key removed only when empty; unknown type refuses the write | Full D1 table, each row asserting a third-party entry survives |
| Untrusted config input — the key is user- and third-party-writable | **Applicable** | Type-checked before use; never assumed to be a string array; non-UTF-8 and empty elements dropped by `normalize` | Wrong `GType`; empty array; array with `NULL` elements; `"::"`; whitespace-only element |
| Untrusted config input — `colors.css` | **Applicable** | Unchanged: `lc_winstore_load()` degrades to an empty store, never crashes | Existing `tests/test-winstore.c` fixtures |
| Process integration — libwnck in a second process | **Applicable** | D3: main-loop-only, own display, no other libwnck client; a fault kills the dialog, not the panel | D2's `snapshot_ok` table (the pure half); the rest is manual |
| Executable-file classification | N/A | No file is classified or executed by content | — |
| Routing | N/A | No routing boundary | — |
| Git / PR automation | N/A | None in this change | — |

## Migration / Rollout

No data migration: `colors.css` keeps `schema=2` and the shipped `.so` is untouched. D1 is itself the
migration story — existing v0.1.0 users hold a scalar, and it stays a scalar.

**The proposal's three slices do not hold against this design; four are needed.** D7 adds core work
to slice 1, and the proposal already flagged its slice 2 (binary + backend + toggle + restart +
`.desktop` + meson + packaging) as the budget risk. Recommended Feature Branch Chain on
`feat/settings-app`, each slice green on `meson test -C build`:

1. Core: `lc-autoload` + `lc-restart` + `lc_winstore_xids()` + their tests. Links no GTK. **High**
   400-line risk on its own (this repo's header-comment density is ~60 % of `lc-winstore.h`); split
   `lc-restart` out if the tasks forecast confirms.
2. Shell + packaging: `main.c` skeleton, `lc-autoload-xfconf.c`, meson target, `.desktop`,
   `debian/control`, `config.yaml`, `POTFILES.in`. No UI logic.
3. Toggle + restart UI: the switch, `property-changed` sync (D5), the confirm dialog, error surfacing.
4. Colour list: `lc-winlist` (D2/D3), rows with titles, single removal, orphan cleanup, `po/`,
   `README.md`, `docs/manual-verification.md`.

Rollback is per slice; reverting all four leaves v0.1.0 behaviour untouched, since nothing the module
loads changes. `sdd-tasks` owns the binding forecast.

## Open Questions

- [ ] Does `xfsettingsd` 4.18 actually honour a `G_TYPE_PTR_ARRAY` `/Gtk/Modules`? D1 makes this
      non-blocking — we never create that shape — but the array read path stays untested against a
      real daemon until manual verification. If it turns out arrays are not honoured at all, D1's
      array row should be revisited in a follow-up, never by converting a live user's key.
- [ ] Icon for the `.desktop` entry: reuse a stock icon name or ship one? Cosmetic, resolvable in
      slice 2.

## D8 — the live/orphan join is pure core, not glue (amendment)

**Decision.** Split `lc-winlist` the same way D1 splits `lc-autoload`: a pure
`src/core/lc-winlist.{c,h}` owning the `snapshot_ok` truth table and the
live/orphan join, plus `src/settings/lc-winlist-wnck.c` as the only part that
touches libwnck — the snapshot capture and the self-XID completeness probe.

**Rejected alternative.** The original placement, a single
`src/settings/lc-winlist.{c,h}`, which made the whole join glue.

**Why this was wrong, and why it matters.** The `stored-color-management` spec
marks four scenarios `(headless-testable)`, including *"Orphaned entry never
appears as a row"* and *"List never marks entries as orphaned under an ambiguous
snapshot"*. Placing the join in `src/settings/` left those scenarios with no unit
test and reachable only by manual inspection — an artifact contradiction that
surfaced during task breakdown, not a preference.

That gap is not cosmetic. This join is the **second caller** of the hazard that
already destroyed real user data in this project: libwnck populates its window
list asynchronously, and a snapshot read too early is empty. If an empty snapshot
is read as "no windows are open", every stored colour is presented as orphaned
and the cleanup action deletes colours the user still wants — this time with the
user's own click on it. The decision rule that prevents that is exactly the kind
of logic that must be pinned by tests rather than checked by eye, and
`src/core/` is where this project keeps testable rules.

The consequence for the boundary is unchanged: `src/core/` stays free of `gtk_`,
`xfconf_` and `wnck_` symbols, which CI enforces by scanning `liblc-core.a`. The
pure module takes the snapshot as data — an obtained flag plus an XID array —
exactly as `lc_winstore_reconcile()` already does, and never calls libwnck itself.
