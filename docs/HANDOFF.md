# Handoff — where this project stands

Written 2026-09-09. Read this first if you are picking the project up on another machine.

## What works today

Milestone **H0** is merged to `main`. There is a working build, a tested pure-logic core, and empirical answers to the questions that would otherwise have been guessed.

```sh
meson setup build && ninja -C build
meson test -C build        # 5 suites, 54 cases
```

## What does NOT work yet

**There is no module.** Nothing loads into the panel, no menu item appears, no colour can be chosen. If you right-click a launcher you get the stock XFCE menu, and that is expected.

Missing: the whole glue layer (`src/glue/`), the detection hook, menu integration, the colour dialog, the store write path, the fingerprint, the CLI, packaging and i18n. That is Phases 3 through 8.

## Three upstream facts this design depends on

Each was verified by reading `xfce4-panel` source at tags 4.16.0, 4.18.0 and master, and each fails **silently** if got wrong — no error, no crash, nothing in a log. That is why they are written down here rather than left in someone's head.

**1. The clickable button is named `launcher-arrow`, not `launcher-button`.**
`launcher_plugin_init()` calls `gtk_widget_set_name(plugin->button, "launcher-button")` and then, in the block that builds the arrow, calls `gtk_widget_set_name(plugin->button, "launcher-arrow")` — targeting `plugin->button` again instead of `plugin->arrow`. Since `gtk_widget_set_name()` overwrites, the final name is `launcher-arrow`, and `plugin->arrow` is never named at all. It is an upstream bug, identical across all three tags, so in practice it is a stable contract.

A rule written against `#launcher-button` matches nothing and paints nothing, with no diagnostic. `tests/test-css.c` carries a regression asserting it is never emitted.

**2. Plugin ids are recycled, and stale xfconf subtrees survive.**
Verified on a live panel: `/plugins/plugin-12` is a launcher still carrying PulseAudio keys; `plugin-13` is a launcher still carrying clock keys. So an orphaned colour left behind when a launcher was removed can be silently inherited by a different launcher that later takes the same id.

That is why a colour binds to the id **plus** a fingerprint of the launcher's desktop-file list, and why reconciliation at load has two independent prune triggers.

**3. `map` is emitted child-before-container.**
On a live panel, across all 22 launchers and both themes tested:

```
[map 001] launcher-arrow  GtkButton
[map 002] launcher-5      XfceLauncherPlugin
```

The design originally claimed the opposite and that claim has been deleted. The practical consequence for the detection hook: **key it on the plugin object, never on the inner button.** A button-keyed hook cannot resolve the plugin id in one pass, because the button maps first. Such an implementation would look correct in a test harness and fail on a real panel.

## Settled by measurement, not argument

- **Provider priority**: `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION` (600) wins, at both screen and widget scope, against the panel's own competing widget-level provider. Widget-level does outrank screen-level at equal priority.
- **Adwaita paints only with `background-image: none`.** Adwaita's `GtkButton` sets an opaque `background-image` that occludes `background-color`, so the rule was correct in every respect except completeness.
- **One base-rule `background-image: none` also covers `:hover`.** Measured by sampling the painted region's mean colour on two themes rather than judging by eye:

  | Theme | base | hover |
  |---|---|---|
  | Mint-Y-Dark-Pink | 64.6 / 44.7 / 51.9 % | 68.9 / 53.3 / 58.1 % |
  | Adwaita | 71.2 / 51.3 / 58.2 % | 74.3 / 61.7 / 64.5 % |

Evidence lives in `docs/spike-findings.md`, `spike/out/crop-*.png` and `spike/out/probe-*.log`. Note that `spike-findings.md` now has a harness portion and a live-panel portion; say which one you mean when quoting it.

## Invariants that are load-bearing

Breaking any of these produces wrong output with no failing test, unless the guard below holds.

| Invariant | Enforced by |
|---|---|
| `src/core/` links glib only — never GTK, libxfce4panel or xfconf | The meson target declares no such dependency; `nm liblc-core.a` shows no matching symbols |
| `LcDocument`'s only constructor is `lc_store_render()` | No `_from_string` or `_from_file` exists. The CSS provider accepts nothing else, so handing it raw file bytes does not compile |
| The provider never sees `colors.css` directly | `gtk_css_provider_load_from_*` appears in exactly one file, `src/glue/lc-provider.c` |
| Reconciliation runs before the provider is attached | The provider takes an `LcDocument`, which can only exist after a store was loaded and reconciled |
| State rules carry `background-color` only; geometry and image suppression live on the base rule | The record parser rejects a state rule carrying anything else |
| An uncomputable fingerprint KEEPS the entry | `lc_store_reconcile_entry()` is a pure truth table under unit test. Only a computed, genuinely mismatching fingerprint may discard |

That last one matters more than it looks: getting the direction backwards means a transient xfconf failure silently wipes every colour the user chose.

## Next step

**Phase 3 (Slice 3, H1-a): Module Entry and Load Path** — tasks 3.1 to 3.6 in `openspec/changes/xfce4-window-button-colors/tasks.md`.

It was started and deliberately stopped before writing any file, so the tree is clean and it can be restarted from scratch. Its runtime attempt is settled as `interrupted`.

Build: `lc-store` load and parse (marker parser, schema=2), `lc_store_render()` returning `LcDocument`, `src/glue/module.c` with the `g_get_prgname()` guard first, and `src/glue/lc-provider.c` as the sole owner of the priority and the only holder of a `GtkCssProvider`.

Then Phase 4 makes the menu appear.

## Testing a module against a live panel

The panel is a live desktop process. Do not write the xfconf `xsettings` `/Gtk/Modules` key: it is session-wide, so every GTK3 application would load the module, and a crash in it takes the panel down.

Use `GTK_MODULES` scoped to the panel process instead, preserving whatever is already there:

```sh
pkill -x xfce4-panel
GTK_MODULES="gail:atk-bridge:/abs/path/module.so" setsid xfce4-panel --display :0.0 &
```

`spike/probe-module.c` is a worked example of a module that is safe to load into a live panel: it guards on `g_get_prgname()`, never calls `g_error`, `g_assert` or `abort` on any reachable path, and its emission hook always returns `TRUE`. Always restore the panel afterwards.

## Known debt

- Design **D18**: the state pseudo-class is appended to the caller's selector inside `lc_css_rule_new()` rather than going through `lc_id_selector_new()` as the sole selector producer. Passes every test and satisfies the spec, but the two should be reconciled.
- **RNF-7 is unproven.** Only 4.18.4 was available; 4.16 and 4.20 need the CI matrix in a later slice.
- The 24-bit fingerprint means roughly a 10⁻⁶ chance per load of keeping an entry that should have been dropped. It fails toward the old behaviour and is cosmetic.
- The fingerprint reads panel-owned xfconf keys with no public API guarantee. If a future panel renames `items`, every entry degrades to "kept unreconciled" — not broken, but the recycled-id bug quietly returns.

## Delivery

17 PRs on a feature-branch chain, tracker `feat/launcher-colors`, re-estimated at roughly 5,635 budget-counted lines. The original estimate of 2,500 was wrong because it counted implementation and treated tests as incidental; under strict TDD the core's test-to-implementation ratio is about 1.4:1. Do not apply that ratio to `src/glue/` (verified manually, few tests), to packaging, or to i18n catalogues.
