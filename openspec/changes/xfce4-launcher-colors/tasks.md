# Tasks: xfce4-launcher-colors

Toolchain is installed and verified (meson 1.3.2, ninja 1.11.1, gcc 13.3.0,
pkg-config 1.8.1, gtk+-3.0 3.24.41, libxfce4panel-2.0/libxfce4util-1.0/
libxfconf-0 4.18.1, gettext 0.21); no install task appears below. Only
`xfce4-panel` 4.18.4 is available locally — RNF-7 (4.16/4.20) stays unproven
until the Slice 7 CI matrix runs; every affected verification step says so.

## Review Workload Forecast (re-estimated after Slice 1 measurement)

**Slice 1 accepted `size:exception`.** Estimated ~300 lines, measured **1025**
(`src/core/*.{c,h}` 407 + `tests/*.c`+`tests/meson.build` 562 + `meson.build`/
`meson_options.txt` 56). Rationale accepted by the user: the ~1.4:1
test-to-implementation ratio is exactly what strict TDD produces for the
unit-testable core layer, the code is not padded, and splitting a single
cohesive core-bootstrap PR further would not have produced independently
buildable halves. See the exception note under Phase 1.

The original ~2,500-line program total is now known to be wrong — it counted
implementation and treated tests as incidental. The re-estimate below counts
implementation, headers, tests, and build-file changes separately per slice,
using Slice 1's measured ~1.4:1 core test:implementation ratio as the
calibration anchor for testable core work, and explicitly **not** applying
that ratio to `src/glue/` (GTK/panel code verified manually, not by unit
tests), packaging metadata, or translation catalogs.

| Field | Value (before) | Value (after re-estimate) |
|-------|-----------------|----------------------------|
| Estimated changed lines | ~2,500 total | ~5,635 budget-counted (~6,335 incl. i18n catalogs, tracked separately) |
| 400-line budget risk | High (Slice 7 at exact boundary) | High — 5 of 8 slices exceed 400 as originally scoped |
| Chained PRs recommended | Yes | Yes — 5 slices split into sub-slices (17 PRs total in the chain) |
| Delivery strategy | auto-chain | auto-chain (unchanged) |
| Chain strategy | feature-branch-chain | feature-branch-chain (unchanged) |

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Slices — before/after estimate (branch / parent / lines / verification)

Existing slice numbers and branch names are unchanged for Slices 1, 2, and 8.
Slices 3, 4, 5, 6, and 7 exceed 400 lines as originally scoped and are split
into sub-slices below; their task numbering (e.g. `4.1`–`4.12`) is untouched —
only which branch each existing task lands on changes. No task content was
edited and no checked-off task was touched.

| # | Slice | Est. lines (before) | Est. lines (after) | Fits 400? | Action |
|---|---|---|---|---|---|
| 1 | H0-a core bootstrap | ~300 | **1025 (actual)** | No | `size:exception` accepted |
| 2 | H0-b spike (no prod code) | ~200 | ~230 | Yes | Keep as-is |
| 3 | H1-a module entry | ~250 | ~570 | No | Split → 3a, 3b |
| 4 | H1-b menu + color | ~350 | ~1,130 | No | Split → 4a, 4b, 4c, 4d |
| 5 | H2-a cleanup + settings | ~350 | ~1,115 | No | Split → 5a, 5b, 5c, 5d |
| 6 | H2-b autoload + CLI | ~300 | ~685 | No | Split → 6a, 6b (6b is tight, ~395) |
| 7 | H3 packaging + CI | ~400 | ~630 | No | Split → 7a, 7b |
| 8 | H4 i18n + polish | ~350 | ~250 budget-counted (+ ~700 catalog, excluded) | Yes | Keep as-is |

Program total (budget-counted, using post-split sub-slice sums): 1025 + 230 +
570 + 1130 + 1115 + 685 + 630 + 250 ≈ **5,635**. Adding the ~700 excluded
i18n catalog lines (see Slice 8 note) gives **~6,335** including everything
authored. This is roughly 2.25×–2.5× the original ~2,500 total — lower than
Slice 1's 3.4× miss because packaging (Slice 7) and glue-heavy slices (3, 4,
6) do not carry Slice 1's core test density, while Slice 5 (fingerprint +
reconciliation core) is expected to match or exceed it.

Per-slice basis for the new numbers:

- **Slice 2** (spike, no lasting tests): throwaway probe module ~120,
  `docs/spike-findings.md` ~70, `meson.build`/option-default tweaks ~20+.
  No unit tests survive the slice (deleted per task 2.7).
- **Slice 3** (mostly `src/core/lc-store` load path + thin glue): store
  load/parse/render is a large, marker-parsing core module — heavier than
  any single Slice-1 module — at an estimated ~150–160 impl lines with
  ~210–220 test lines (ratio ≈1.4, consistent with the anchor); `module.c` +
  `lc-provider.{c,h}` glue add ~170–180 impl lines with no dedicated unit
  tests (manual-only per module-lifecycle spec).
- **Slice 4** (fingerprint + store-write core, plus detection/menu/dialog
  glue): `lc-fingerprint` and the `lc-store` write-path extension are core
  and follow the ~1.4 ratio (~200 impl / ~315 test lines combined); `lc-hook`,
  `lc-menu`, and `lc-dialogs` are `src/glue/` — GTK/panel code verified
  manually — so their ~380 impl lines carry only ~130 test lines (the
  headless-testable filter ladder and dedup check), a ratio well under 1.4,
  called out explicitly rather than inflated to match core.
- **Slice 5** (reconciliation is the heaviest core work in the program):
  `lc_store_reconcile_entry()`'s D15 truth table and `lc_store_prune()`
  (D12) are estimated at ~130 impl / ~200 test lines — a ≈1.54 ratio, at or
  above the anchor as expected for the fail-safe direction under test.
  Parser hardening and `lc-settings` add a further ~150 impl / ~250 test
  lines at a similar-or-higher ratio. The remaining glue (shared xfconf
  call, write-back gating wiring, settings dialog, `GFileMonitor`) is ~250
  impl lines with much lighter, partly-testable coverage (~90 lines, since
  the shared-call count and the write-gating predicate are the only glue
  pieces worth a unit test here).
- **Slice 6** (autoload core is testable; the CLI binary is unusually
  testable for `src/cli/` because it links no GTK): `lc-autoload` core adds
  ~80 impl / ~130 test lines (ratio ≈1.6); the CLI (`src/cli/main.c`,
  5 subcommands) adds ~180 impl lines with ~200 test lines, because CLI
  logic — unlike panel glue — is exercised headlessly against a mocked
  xfconf backend, not manually. `lc-autoload-xfconf.c` (the actual xfconf
  backend) is glue with no dedicated test of its own.
- **Slice 7** (packaging is verbose and almost untested by design):
  `debian/` control files ~150–190, `install.sh` ~120–180,
  PKGBUILD/RPM spec ~60–100, CI workflow YAML ~120–180, plus small
  lint/dependency-audit scripts ~30–50. Only the audit/lint configuration
  is scripted; there is no unit-test multiplier here, matching the
  packaging-distribution spec's own "CI-verified, not a Meson unit test"
  annotations.
- **Slice 8** (i18n and docs — counting method stated explicitly): `README.md`
  ~150, gettext domain-binding wiring in existing glue ~25, the catalog
  completeness test ~50, `meson.build` i18n wiring ~25 — **all counted**
  toward the 400-line budget (≈250 total, Low risk). The `.pot`/`es.po`/
  `en.po` catalogs themselves (~700 lines combined, scaling with the number
  of translatable strings) are **excluded from the 400-line review budget**,
  the same convention used for generated goldens: they are formulaic,
  translation-reviewed rather than code-reviewed, but are still tracked here
  for completeness and remain part of the repository's authored content.

### Recommended slice splits

Splits follow the same reasoning accepted for Slice 1: split when the halves
are independently buildable and testable on their own; do not split when it
would produce a PR that cannot build or be verified in isolation. Existing
task numbers (e.g. `3.1`, `4.9`) are unchanged — each row below only assigns
existing tasks to a branch.

| Sub-slice | Contains tasks | Branch | Parent | Est. lines | Rationale |
|---|---|---|---|---|---|
| 3a | 3.1–3.2 | `feat/launcher-colors-03a-store-load` | `feat/launcher-colors-02-spike-probe` | ~380 | Pure core (`lc-store` load/parse/render); builds and tests standalone with `meson test`, no panel headers needed |
| 3b | 3.3–3.6 | `feat/launcher-colors-03b-module-provider` | 3a | ~190 | Glue (`module.c`, `lc-provider.c`); needs 3a merged, needs a live panel for its manual verification |
| 4a | 4.1–4.2 | `feat/launcher-colors-04a-fingerprint` | 3b | ~270 | Pure core (`lc-fingerprint`); standalone-testable, no dependency on 4b–4d |
| 4b | 4.3–4.4 | `feat/launcher-colors-04b-store-write` | 4a | ~270 | Pure core (`lc-store` write path); standalone-testable |
| 4c | 4.5–4.8 | `feat/launcher-colors-04c-detection-hook` | 4b | ~300 | Glue (`lc-hook`); menu insertion call is stubbed (`TODO`, no-op) until 4d — still builds and the filter-ladder/dedup tests still run headless |
| 4d | 4.9–4.12 | `feat/launcher-colors-04d-menu-dialogs` | 4c | ~290 | Glue (`lc-menu`, `lc-dialogs`, apply-flow wiring); replaces 4c's stub with the real insertion call; needs a live panel for verification |
| 5a | 5.1–5.2 | `feat/launcher-colors-05a-reconcile-prune` | 4d | ~340 | Pure core (D15 truth table + D12 prune); standalone-testable |
| 5b | 5.3–5.4 | `feat/launcher-colors-05b-parser-hardening` | 5a | ~190 | Pure core (marker-edge-case fixtures); same file as 5a, sequenced to avoid merge conflicts |
| 5c | 5.7–5.8 | `feat/launcher-colors-05c-settings-core` | 5b | ~230 | Pure core (`lc-settings`); independent of reconciliation, standalone-testable |
| 5d | 5.5, 5.6, 5.9, 5.10, 5.11 | `feat/launcher-colors-05d-reconcile-wiring` | 5c | ~355 | Glue (shared xfconf call, write-back gating, settings dialog, `GFileMonitor`); needs 5a and 5c merged; live-panel recycled-id verification |
| 6a | 6.1–6.3 | `feat/launcher-colors-06a-autoload-core` | 5d | ~290 | Pure core + xfconf backend; standalone-testable via fake backend |
| 6b | 6.4–6.10 | `feat/launcher-colors-06b-cli` | 6a | ~395 | CLI binary; tight against the budget — if it grows, split further into CLI-core-commands vs. `--enable`/`--disable`+spawn-vector |
| 7a | 7.1–7.2 | `feat/launcher-colors-07a-packaging-linux` | 6b | ~335 | `debian/` + `install.sh`; buildable/installable standalone via `dpkg-buildpackage` |
| 7b | 7.3–7.6 | `feat/launcher-colors-07b-packaging-ci` | 7a | ~295 | PKGBUILD/spec + CI workflow; its `dpkg-deb -I` check depends on 7a already being merged up-chain |

Slice 8 (`feat/launcher-colors-08-i18n-polish`) now targets 7b instead of the
original single Slice 7 branch; its own line estimate (~250 budget-counted)
stays under 400 and is not split.

Milestone reconciliation: H0 = slices 1–2; H1 = slices 3–4 (now 3a/3b, 4a–4d);
H2 = slices 5–6 (now 5a–5d, 6a/6b); H3 = slice 7 (now 7a/7b); H4 = slice 8.

---

## Phase 1 — Slice 1 (H0-a): Core Bootstrap

**`size:exception` accepted for this slice.** Measured at 1025 changed lines
against an estimated ~300 (`src/core` 407 + `tests` 562 + build files 56).
Rationale: the ~1.4:1 test-to-implementation ratio is exactly what strict TDD
produces for the unit-testable core layer, the diff is not padded, and this
is the first PR bootstrapping both the build system and the test harness —
there is no smaller cohesive, independently buildable unit to split it into.
Completed tasks below are unchanged.

- [x] 1.1 Create `meson.build` (project, `c_std=c11`, GPL-2.0-or-later, `static_library('lc-core', deps: [glib, gio])`, `tests/` linking `lc-core` only) and `meson_options.txt` with the `provider_priority` combo option (`application|user`, D7).
- [x] 1.2 Edit `openspec/config.yaml`: flip `strict_tdd: false` to `strict_tdd: true` in this same PR (D1, TDD gate).
- [x] 1.3 RED: `tests/test-guard.c` asserting `lc_guard_should_activate()` for `"xfce4-panel"` → true; `NULL`, `""`, `"gedit"`, `"xfce4-panel-wrapper"`, `"Xfce4-Panel"` → false (module-lifecycle spec, RNF-6 scenario).
- [x] 1.4 GREEN: `src/core/lc-guard.{c,h}` implementing `lc_guard_should_activate()`.
- [x] 1.5 RED: `tests/test-id.c` for `lc_id_is_valid`, `lc_id_parse`, and `lc_id_selector_new()` producing `"#launcher-<id> #launcher-arrow"` (D5), plus a regression asserting `#launcher-button` is never emitted.
- [x] 1.6 GREEN: `src/core/lc-id.{c,h}`.
- [x] 1.7 RED: `tests/test-color.c` for `lc_color_parse`/`lc_color_to_css`/`lc_color_to_hex` round-trip on valid and malformed `#rrggbbaa` input.
- [x] 1.8 GREEN: `src/core/lc-color.{c,h}`.
- [x] 1.9 RED: `tests/test-css.c` for `lc_css_rule_new`/`lc_css_render_rules`/`LcStyle` rendering the `#launcher-<id> #launcher-arrow { ... }` shape from `launcher-color-persistence` spec's "Generated rule uses the two-level selector" scenario. (Renamed from `lc_css_document_new` per design amendment: this module renders CSS fragments only and must not be mistaken for a constructor of the later `LcDocument` opaque type — see `lc-css.h` header comment.)
- [x] 1.10 GREEN: `src/core/lc-css.{c,h}`.
- [x] 1.11 RED: `tests/test-paths.c` for XDG path resolution with `$HOME` unset (module-lifecycle step 3 failure path — one `g_warning`, no crash).
- [x] 1.12 GREEN: `src/core/lc-paths.{c,h}`.
- [x] 1.13 Verify: `meson setup build && meson test -C build` — all above green; confirm `lc-core` links only `glib`/`gio`, no GTK symbol present.

## Phase 2 — Slice 2 (H0-b): Spike Probe — Output Is a Finding, Not Production Code

Do not close the two open provider-priority/paint questions in `design.md`
(read-only) on paper. This slice records empirical evidence only.

**Standalone-harness portion complete; live-panel portion deferred.**
Tasks 2.2–2.4's literal action (loading anything via `/Gtk/Modules` on the
live panel) is out of scope without explicit user consent — the hard
prohibition on touching `/Gtk/Modules` or disturbing the running panel
still applies. `spike/harness.c` answers the priority/paint/hover
questions empirically in a standalone GTK3 process instead (see
`docs/spike-findings.md`); 2.2–2.4 stay unchecked pending the deferred
live-panel step.

- [x] 2.1 Reproduce the `map`-emission-hook logic in a standalone harness (`spike/harness.c`, not under `src/glue/`, no meson target, never installed) that registers one `map` emission hook and logs, per mapped widget of interest: order and identity, to sanity-check D4's no-ancestor-walk assumption. (Adapted from a loadable `gtk_module_init` probe to a self-contained standalone program per this slice's explicit standalone-harness scope; a loadable probe module is deferred to the live-panel step.)
- [x] 2.2 Load a probe via `/Gtk/Modules` on the live 4.18.4 panel; attach a `GtkCssProvider` at `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION` (600) against `#launcher-<id> #launcher-arrow`; record whether the background visibly paints over Adwaita and the Xubuntu default theme, and whether hover still works. **Completed** by the consented live-panel probe (`spike/probe-module.c`, scoped to the panel process via `GTK_MODULES`, never the session-wide xfconf key). Evidence: `spike/out/probe-{adwaita,mint}.log` (colour applied to `launcher-5` at APPLICATION (600), screen scope; PRELIGHT forced) and `spike/out/crop-live-{adwaita,mint}-{base,hover}.png`. Both themes paint and hover survives; `background-image: none` also suppresses the theme image on `:hover`. Checkbox flipped after the fact — the task text still said "deferred" long after the probe had run.
- [ ] 2.3 **Deferred — requires live-panel consent, out of scope for this batch.** Repeat 2.2 at `GTK_STYLE_PROVIDER_PRIORITY_USER` (800); record the same observations plus whether it now ties with the user's own `gtk.css`. Standalone-harness equivalent completed instead: see `docs/spike-findings.md` Q1.
- [ ] 2.4 **STILL OPEN — and this is the ONLY task in the whole plan that covers RNF-2 or RNF-3.** Measure hook-callback cost against RNF-2 (cheap filter path) and RNF-3 (<10 ms) with the panel's real launcher count. No synthetic substitute was attempted in the harness, and the consented live probe recorded no timing data (`grep` for ms/elapsed in `spike/out/probe-*.log` returns nothing). The blocker named in the original wording is gone: the live probe established the real count at **21 launchers** on this panel. **If this task is never run, the module ships with its two performance requirements entirely unverified** — no other task in phases 1–8 asserts either one. Schedule it against the real `lc-hook` in slice 4c rather than a throwaway probe, since 4c is where the filtering ladder actually exists.
- [x] 2.5 Record findings in `docs/spike-findings.md` (which priority wins, paint/hover result per theme, map-hook targeting result — cost measurement explicitly marked deferred per 2.4) and cross-reference it from `design.md` (read-only) without checking off its Open Questions items.
- [x] 2.6 Set `provider_priority`'s default value in `meson_options.txt` (from slice 1) to match the winning priority found in the standalone harness — the only production-adjacent change in this slice. Winning priority: `application` (already the slice-1 default; only the option comment was updated from "placeholder" to "empirically confirmed").
- [x] 2.7 Confirm the throwaway harness never enters the build/install tree (no meson target references `spike/`, built manually via a direct `gcc` invocation only, documented in `docs/spike-findings.md`); confirm `meson test -C build` from slice 1 is unaffected (5/5 test binaries, still the same 27 assertions, all green).

## Phase 3 — Slice 3 (H1-a): Module Entry and Load Path

- [x] 3.1 RED: `tests/test-store-load.c` — `LcStore` load/parse of a well-formed `colors.css` with `/* id=<n> fp=<hex6> */` markers (schema=2), and of a missing/unreadable/malformed file degrading to an empty store with one warning (`launcher-color-persistence` "Malformed Configuration Degrades Safely").
- [x] 3.2 GREEN: `src/core/lc-store.{c,h}` — load/parse only (record-oriented marker parser); `lc_store_render()` for full-document rendering incl. markers (D16 groundwork).
- [x] 3.3 Create `src/glue/module.c` with `gtk_module_init`/`gtk_module_exit`: guard first (1.4), static `initialized` flag idempotency, `lc_settings_load()` before any render (lifecycle step 4 — stub default-only settings for now).
- [x] 3.4 Create `src/glue/lc-provider.{c,h}` — **sole** owner of `LC_PROVIDER_PRIORITY` and `lc_provider_attach()` (D7), using the priority chosen in 2.6; register the provider from `lc_store_render()` output via `load_from_data()` only — never `load_from_file(colors.css)` (D8).
- [x] 3.5 Wire `module.c` to call `lc_store_load()` → `lc_store_render()` → `lc_provider_attach()`/`load_from_data()` at init; a `GError` from CSS parsing discards the rule set with one warning, never propagated (lifecycle step 7).
- [x] 3.6 Verify: `meson test -C build`; manual — load via `/Gtk/Modules` on 4.18.4, confirm the guard is inert in another GTK3 app (e.g. `gedit`), confirm no crash on a hand-corrupted `colors.css`.

## Phase 4 — Slice 4 (H1-b): Detection, Menu, Colour Apply (RF-1…RF-5, RF-9)

- [ ] 4.1 RED: `tests/test-fingerprint.c` — determinism; **order-insensitivity** (shuffled same set → same fp); sensitivity (one element replaced → different fp); empty/`NULL`/whitespace-only elements; **length-framing regression: `["ab","c"]` vs `["a","bc"]` must produce different fingerprints**; non-UTF-8 and embedded-separator bytes; output is exactly 6 lowercase hex chars.
- [ ] 4.2 GREEN: `src/core/lc-fingerprint.{c,h}` — byte-wise `strcmp` sort (not locale collation), length-prefixed `<len>:<bytes>` framing, `G_CHECKSUM_SHA256` truncated to 6 lowercase hex chars.
- [ ] 4.3 RED: extend `tests/test-store-load.c` (or new `tests/test-store-write.c`) for `lc_store_set`/`lc_store_has`/`lc_store_save` writing the `/* id=<n> fp=<hex6> */` marker immediately before its rule, and round-tripping N entries unmodified.
- [ ] 4.4 GREEN: extend `src/core/lc-store.{c,h}` with `lc_store_set`, `lc_store_has` (drives RF-5 visibility), `lc_store_save` (atomic write+rename).
- [ ] 4.5 Create `src/glue/lc-hook.{c,h}` — one `g_signal_add_emission_hook` on `map` (`g_signal_lookup("map", GTK_TYPE_WIDGET)`), the 7-step filtering ladder (Order 1–7 in design.md, read-only) with `lc_hook_resolve_plugin()` matching the plugin GObject first and the ancestor walk only as a compiled-in fallback per the 2.5 finding; dedup via `g_object_set_data_full(plugin, "lc-attachment", ...)` (D11) checked before insertion **and** registered with a destroy notify.
- [ ] 4.6 RED: `tests/test-hook-filter.c` (headless, mock `GtkWidget`/name inputs) for the cheap-filter ladder: non-`l`-prefixed name discarded at step 2; non-`XFCE_IS_PANEL_PLUGIN` discarded at step 4; unexpected widget types never dereferenced past their type check.
- [ ] 4.7 GREEN: filtering-ladder logic isolated from live GTK types where feasible so 4.6 runs headless; wire the remainder into `lc-hook.c`.
- [ ] 4.8 Add a dedicated test (in `tests/test-hook-filter.c` or a new `tests/test-dedup.c`) asserting a second `map` emission on the same plugin instance (panel autohide/show re-fire, D11) does not insert menu items twice.
- [ ] 4.9 Create `src/glue/lc-menu.{c,h}` — insert "Color de fondo…" and "Quitar color" at detection time only, inside `if (menu == NULL)` (D9); never insert on popup; `gtk_widget_set_no_show_all(TRUE)` + `gtk_widget_set_visible()` driven by `lc_store_has()` for RF-5 (D10); **never call `xfce_panel_plugin_menu_destroy()`** anywhere in this file.
- [ ] 4.10 Create `src/glue/lc-dialogs.{c,h}` (colour chooser only in this slice) — `GtkColorChooserDialog` with palette swatches and an RGBA editor with alpha (RF-2).
- [ ] 4.11 Wire menu item activation → dialog → glue reads `/plugins/plugin-<id>/items` from xfconf → `lc_fingerprint_compute()` (fresh fp per D14, never carried over) → `lc_store_set` → `lc_store_render` → `load_from_data` (visual apply, before disk write) → `lc_store_save` → `lc_menu_refresh_visibility`.
- [ ] 4.12 Verify: `meson test -C build`; manual — right-click shows "Color de fondo…" above "Propiedades" on the first popup after detection; colour applies in <100 ms with no flicker; "Quitar color" hidden when uncoloured and visible when coloured; confirm no other module's menu items disappear.

## Phase 5 — Slice 5 (H2-a): Reconciliation, Stale Cleanup, Settings (RF-7, RF-8)

- [ ] 5.1 RED: `tests/test-reconcile.c` — the full D15 truth table, one case per row: `id_is_launcher=false` → `DROP_NOT_LAUNCHER`; `items_readable=false` (any reason) → `KEEP`; `items_readable=true` + absent/malformed marker → `KEEP`; present marker + equal fp → `KEEP`; present marker + differing fp (**only** when successfully computed) → `DROP_FINGERPRINT`. Assert `DROP_FINGERPRINT` is reachable **only** via a genuine computed mismatch, never via an unreadable/missing/malformed input.
- [ ] 5.2 GREEN: `src/core/lc-store.{c,h}` — add `lc_store_reconcile_entry()` (pure, D15) and `lc_store_prune(store, live_ids, n)` (pure, D12).
- [ ] 5.3 RED: extend `tests/test-store-load.c` with the marker edge cases from the threat matrix (read-only, `design.md`): absent marker, malformed marker (non-hex fp, over-long fp, mismatched `id=`), marker with no following rule, rule with no marker — each entry kept-but-unverifiable per the record-oriented parser, never crashing the parse.
- [ ] 5.4 GREEN: harden the `lc-store.c` marker-record parser for 5.3's fixtures under `tests/data/`.
- [ ] 5.5 Add glue in `src/glue/module.c` (or a new `src/glue/lc-reconcile.c`) issuing **exactly one** `xfconf_channel_get_properties(channel, "/plugins")` call at load, shared by both the D12 live-id list and the D14 per-entry `items` fingerprint lookup — never one D-Bus round trip per launcher (assert via a fake-backend test counting calls).
- [ ] 5.6 Wire load-time reconciliation per D16: reconcile the in-memory `LcStore` first, feed the provider the pruned document via `lc_store_render()`/`load_from_data()`, and only rewrite `colors.css` afterward **if and only if** step 5.2's prune actually dropped an entry (lifecycle step 8 — no write, no mtime touch, when nothing changed).
- [ ] 5.7 RED: `tests/test-settings.c` — defaults when `settings.ini` is absent; out-of-range radius (e.g. `999`) clamped to 0–12, not rejected; malformed file falls back to defaults with one warning.
- [ ] 5.8 GREEN: `src/core/lc-settings.{c,h}`.
- [ ] 5.9 Extend `src/glue/lc-dialogs.{c,h}` with the radius/margin settings dialog (RF-7), applying changes to every coloured launcher via a full re-render.
- [ ] 5.10 Add a `GFileMonitor` on `colors.css` (250 ms debounce) so CLI edits (Slice 6) apply live without requiring `xfce4-panel -r`.
- [ ] 5.11 Verify: `meson test -C build` incl. `test-fingerprint`, `test-reconcile`; manual **recycled-id scenario** (design.md testing table, read-only): colour a launcher, unload the module, remove that launcher, add a different one that takes the recycled id, reload — the new launcher MUST render uncoloured. Also verify a session with no changes leaves `colors.css`'s mtime untouched.

## Phase 6 — Slice 6 (H2-b): Autoload and CLI (RF-6, RF-10)

- [ ] 6.1 RED: `tests/test-autoload.c` — `lc_autoload_add/remove/is_enabled` over a fake `gchar**` backend: idempotent add (no duplicate), remove preserves other modules, string vs string-array shape preserved on write.
- [ ] 6.2 GREEN: `src/core/lc-autoload.{c,h}` (pure list transforms) + the `LcAutoloadBackend` 2-function vtable (D13).
- [ ] 6.3 Create `src/glue/lc-autoload-xfconf.c` implementing `LcAutoloadBackend` against the `xsettings` channel's `/Gtk/Modules` key, normalizing string and string-array shapes to `gchar**`.
- [ ] 6.4 Create `src/cli/main.c` (links `lc-core` + xfconf backend only, **not GTK**, never `dlopen`s the module): `set <id> <#rrggbbaa>`, `unset <id>`, `list`, `--enable`, `--disable`.
- [ ] 6.5 RED: `tests/test-cli-set.c` (or equivalent) — `set` rejects a malformed `#rrggbbaa` value without writing; `set` rejects an `<id>` absent from a mocked xfconf `/plugins` lookup without writing; `set` on a valid id+colour writes the `/* id=<n> fp=<hex6> */` marker via the same `lc_fingerprint_compute()`/`lc_store_set()` path the GUI uses.
- [ ] 6.6 GREEN: wire `set`'s xfconf `items` lookup and fingerprint computation into `main.c`, reusing `lc-core` — no duplicated logic.
- [ ] 6.7 RED: `tests/test-cli-unset-list.c` — `unset <id>` removes only the targeted entry; `list` prints every stored id/colour pair, one per line.
- [ ] 6.8 GREEN: wire `unset`/`list` in `main.c`.
- [ ] 6.9 Wire `--enable`/`--disable` to `lc_autoload_add`/`remove` via the xfconf backend, then `g_spawn_async` with an argv **vector** (never `g_spawn_command_line_*`) to run `xfce4-panel -r`; RED test asserting the argv builder never returns a shell string (`"sh -c"` absent), and a non-zero panel-reload exit warns without leaving partial autoload state.
- [ ] 6.10 Verify: `meson test -C build`; manual — `--enable` then a new session shows the module active; `--disable` then a new session shows the native panel with `colors.css` untouched.

## Phase 7 — Slice 7 (H3): Packaging and CI Matrix (RNF-4, RNF-5, RNF-7, RNF-9)

- [ ] 7.1 Add `debian/` control files declaring `Depends: libgtk-3-0, libxfce4panel-2.0-4, libxfce4util7, libxfconf-0-3, xfce4-panel (>= 4.16)` only, and a GPL-2.0-or-later license field consistent with source headers.
- [ ] 7.2 Add `install.sh` (host package-manager detection, dependency report, `meson setup`/`ninja install`, `--user` flag installing into `~/.local`).
- [ ] 7.3 Add a PKGBUILD/RPM spec mirroring the same dependency set.
- [ ] 7.4 Add GitHub Actions workflow: build + `meson test -C build` on 4.16 / 4.18 / 4.20 containers, `dpkg-deb -I` dependency check, package-size check (<100 KB), `lintian`, and a dependency audit asserting no Python/Electron/Qt.
- [ ] 7.5 Note in the workflow and in `docs/manual-verification.md` (read-only reference target from design.md) that this is the **first point RNF-7 (4.16/4.20) is actually verified** — it is unproven on this machine.
- [ ] 7.6 Verify: CI matrix green on all three panel versions; `dpkg-deb -I` shows only the approved `Depends`; measure and record final `.deb` size. If packaging metadata cannot split under 400 lines after one honest slicing pass, request `size:exception` per the proposal's own risk note rather than trimming comments/tests.

## Phase 8 — Slice 8 (H4): Localization and Polish (RF-11)

- [ ] 8.1 Mark every user-facing string (menu items, dialog labels, CLI help/error text) for gettext extraction; generate `po/xfce4-launcher-colors.pot`.
- [ ] 8.2 Add `po/es.po` and `po/en.po` covering every extracted string.
- [ ] 8.3 RED: a catalog-completeness check (e.g. `msgfmt --statistics`) wired into `meson test -C build` asserting zero missing entries in either catalog against the `.pot`.
- [ ] 8.4 GREEN: fill any gap 8.3 reports; wire the gettext domain binding so a new `.po` file needs no source change (extensibility requirement).
- [ ] 8.5 Write `README.md` (install, usage, `--enable`/`--disable`, rollback via `--disable` → uninstall → revert).
- [ ] 8.6 Verify: `meson test -C build` incl. the catalog check; manual — switch session locale to `es_ES` and `en_US` and confirm menu/dialog/CLI strings render translated; confirm the H3 CI matrix (Phase 7, read-only) still passes unchanged.

## Key Learnings

1. The design's D14/D15 fingerprint-and-reconcile logic naturally lands in Slice 5 (H2-a) because RF-8 is scoped there, not Slice 1, despite `lc-fingerprint`/`lc-store` reconciliation being pure core code with no GTK dependency.
2. Slice 2 must produce evidence (a findings note plus a one-line meson-option default), never production code, and must not check off `design.md`'s Open Questions items.
3. The D12+D14 shared `xfconf_channel_get_properties("/plugins")` call needs its own call-count test to guard against silently regressing to 22 D-Bus round trips.
4. `lc_store_reconcile_entry()`'s truth table is the fail-safe direction under test: `DROP_FINGERPRINT` must only be reachable through a genuinely computed mismatch.
5. RNF-7 (4.16/4.20 compatibility) stays formally unproven until Slice 7's CI matrix runs; no task in Slices 1–6 may claim to verify it.
6. Slice 1's measured 1025 lines (3.4× the original ~300 estimate) showed the ~1.4:1 test:implementation ratio from strict TDD, not padding; re-estimating Slices 2–8 with that ratio applied only to testable `src/core/` work — and explicitly not to `src/glue/`, packaging, or i18n catalogs — raised the program total from ~2,500 to ~5,635 budget-counted lines and required splitting five of the remaining seven slices.
