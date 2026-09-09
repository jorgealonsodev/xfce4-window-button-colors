# Proposal: xfce4-launcher-colors

Source of truth: `PRD_xfce4-launcher-colors.md` v1.1 (Spanish). Evidence: `research.md`.
Where `explore.md` claims a `#launcher-button` selector, research disproved it — ignore that claim.

## Intent

XFCE lets users stack several identical launchers on the panel and offers no native way to tell them
apart (PRD §2). The only workaround today is hand-editing `~/.config/gtk-3.0/gtk.css` while knowing
the plugin's internal id — inaccessible to most users. Success: right-click a launcher, pick a
background colour, see it applied instantly and restored on next login, with zero extra processes.

## Scope

### In Scope (by milestone → PRD requirement IDs)

| Milestone | Delivers | IDs |
|---|---|---|
| H0 — Spike | Toolchain install, Meson/Ninja bootstrap, test harness, throwaway probe answering §7.4's five open points | prerequisite for all |
| H1 — MVP | Menu items, colour chooser, instant apply, remove-colour, persistence, move-safe ids | RF-1…RF-5, RF-9, RNF-1…RNF-3, RNF-6, RNF-8 |
| H2 — Autoload, settings, CLI | `/Gtk/Modules` toggle, style settings, stale-entry cleanup, CLI | RF-6, RF-7, RF-8, RF-10 |
| H3 — Packaging | `.deb` (amd64/arm64), `tar.gz` + `install.sh`, PKGBUILD/spec, CI | RNF-4, RNF-5, RNF-7, RNF-9 |
| H4 — Polish | gettext es/en, README, distro test matrix | RF-11 |

### Out of Scope (PRD §3 non-objectives, verbatim in intent)

- Colouring panel plugins that are not launchers (clock, tray, etc.).
- Panels other than `xfce4-panel` (LXQt, MATE…).
- Changing the icon itself — background only.
- Wayland-specific support (expected to work, not actively tested in v1).

## Capabilities

### New Capabilities

- `launcher-color-menu`: context-menu items, colour chooser dialog, instant apply, conditional
  remove-colour visibility (RF-1…RF-5).
- `launcher-color-persistence`: `colors.css` schema, generation, read/write, stale-entry cleanup,
  id stability across moves (RF-4, RF-8, RF-9).
- `module-lifecycle`: GTK module entry, `xfce4-panel`-only guard, CSS provider registration and
  reload, launcher detection hook, `/Gtk/Modules` autoload toggle (RF-6, RNF-1…RNF-3, RNF-6).
- `style-settings`: `settings.ini` handling and the radius/margin dialog (RF-7).
- `launcher-colors-cli`: `set` / `unset` / `list` / `--enable` / `--disable` (RF-10).
- `packaging-distribution`: `.deb`, `tar.gz` + `install.sh`, CI matrix (§9, RNF-4, RNF-5, RNF-9).
- `localization`: gettext catalogues for Spanish and English (RF-11).

### Modified Capabilities

None — greenfield repository, `openspec/specs/` is empty.

## Approach

Architecture **option A** (PRD §7.1), settled on documentary evidence; Plan B is not triggered.

| Decision | Settled form |
|---|---|
| Load mechanism | In-process GTK3 module in `xfce4-panel`, registered in the xsettings `/Gtk/Modules` key |
| Menu integration | Public `xfce_panel_plugin_menu_insert_item()`, unchanged since 4.16.0 |
| Launcher detection | One `g_signal_add_emission_hook()` on `GtkWidget::map`, registered once at module init, with cheap filtering in the callback |
| CSS selector | `#launcher-<id> #launcher-arrow` — the inner button's effective name is `launcher-arrow` because of a verified upstream naming bug, identical across 4.16.0 / 4.18.0 / `master` |
| Menu item timing | Insert on launcher detection, never on popup — the plugin menu is built once and cached |
| RF-5 visibility | `gtk_widget_set_no_show_all()` + `gtk_widget_set_visible()` on the already-inserted item; never re-insert, never call `xfce_panel_plugin_menu_destroy()` (it frees other modules' items too) |

### Testability split

Isolate pure logic from GTK/panel glue so the former is unit-testable under `meson test` without a
running panel.

| Unit-testable (Meson) | Manual verification against a live panel |
|---|---|
| CSS rule generation and selector shape | Module load via `/Gtk/Modules` |
| Launcher id parsing and validation | Emission hook finding real plugin instances |
| Hex/RGBA parsing and validation | Provider priority winning over the active theme |
| `colors.css` read/write and RF-8 stale-entry cleanup | Visual paint and hover behaviour |
| `settings.ini` read/write | Cross-version behaviour on 4.16 / 4.18 / 4.20 |

### TDD gate

`strict_tdd` is currently `false` in `openspec/config.yaml`, fail-closed because no build system
exists. **It MUST flip to `true` in the same slice that lands the Meson test scaffolding** (slice 1),
and every later slice is written test-first for its unit-testable core.

## Delivery plan

Total work far exceeds the 400-line review budget. Strategy: **Feature Branch Chain** — the module is
not user-usable until H1 completes and packaging depends on it, so slices integrate on a tracker
branch before main. Tracker: `feat/launcher-colors` (draft, no-merge). Slice 1 targets the tracker;
each later slice targets its immediate predecessor.

| # | Slice | Scope | Est. changed lines |
|---|---|---|---|
| 1 | H0-a | `meson.build`, test harness, pure-logic core (CSS generation, id/colour validation) + unit tests; flip `strict_tdd: true` | ~300 |
| 2 | H0-b | Throwaway probe module: `map` hook, provider at `APPLICATION` vs `USER`, findings recorded | ~200 |
| 3 | H1-a | Module entry, `g_get_prgname()` guard, provider registration, `colors.css` load, silent abort | ~250 |
| 4 | H1-b | Detection hook, menu insertion, colour chooser, instant reload, remove-colour (RF-1…RF-5, RF-9) | ~350 |
| 5 | H2-a | `colors.css` write path, RF-8 cleanup, `settings.ini`, style dialog (RF-7, RF-8) | ~350 |
| 6 | H2-b | Autoload toggle via xfconf `/Gtk/Modules` (RF-6) + CLI (RF-10) | ~300 |
| 7 | H3 | `debian/`, `install.sh`, PKGBUILD/spec, GitHub Actions matrix | ~400 |
| 8 | H4 | gettext es/en, README, distro test matrix | ~350 |

Each slice has an independent verification step and its own rollback (revert that PR only).
Slice 7 is the likeliest `size:exception` candidate if packaging metadata cannot split cleanly.

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `meson.build`, `tests/` | New | Build and unit-test scaffolding (slice 1) |
| `src/` (pure logic) | New | CSS generation, id/colour parsing, config I/O |
| `src/` (GTK glue) | New | Module entry, guard, hook, menu, dialogs |
| `src/cli/` | New | `xfce4-launcher-colors` binary (RF-10) |
| `debian/`, `install.sh`, `po/` | New | Packaging and i18n |
| `openspec/config.yaml` | Modified | `strict_tdd: false → true` in slice 1 |
| `~/.config/xfce4-launcher-colors/` | New (runtime) | `colors.css`, `settings.ini` — user data only |
| xsettings `/Gtk/Modules` | Modified (runtime) | Autoload toggle; user-scoped, reversible |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| CSS selector matches but nothing visibly paints | Med | H0-b probes against Adwaita and the Xubuntu default theme before any MVP code |
| Screen-level provider loses to the panel's widget-level `APPLICATION` provider | Med | Empirical decision in H0-b; `USER` (800) is the fallback, at the cost of tying with the user's `gtk.css` |
| Wrong selector (`#launcher-button`) fails silently | Low | Effective name is `launcher-arrow`; documented in code and covered by a CSS-generation unit test |
| Menu item inserted too late and never appears | Med | Insert on detection, not on popup; never use `menu_destroy()` to refresh |
| Emission hook cost, since it fires for every mapped widget | Med | Cheap name-prefix / `XFCE_IS_PANEL_PLUGIN` filter first; cost measured in H0-b against RNF-2 |
| `/Gtk/Modules` loads the `.so` into every GTK3 app | Low | Immediate return when `g_get_prgname() != "xfce4-panel"`; documented |
| Module fault crashes the panel (RNF-6) | High impact | See rollback: guard, defensive checks, no aborts |
| `xfce4-panel` migrates to GTK4 (no confirmed date) | High impact / Low near-term | 4.20.x is still GTK 3.24; CSS persistence stays valid. Track the XFCE development branch. Do not plan around the unevidenced "4.22+" date |
| Toolchain absent on this machine | High | Named as an explicit prerequisite of slice 1 (below) |
| 4.16 / 4.20 verification impossible here | Med | Requires environments not available on this machine; deferred to the H3 CI matrix |
| Distro-patched headers diverge from upstream | Low | Diff installed headers after `libxfce4panel-2.0-dev` lands |

## Rollback Plan

**RNF-6 — never crash the panel, abort silently.** Concretely:

- `gtk_module_init` returns immediately unless `g_get_prgname()` equals `xfce4-panel`; nothing else
  is initialised in any other process.
- No `g_error()`, `g_assert()`, or `abort()` on any shipped code path; failures use `GError`
  out-parameters and at most one `g_warning`.
- The emission-hook callback type-checks with `GTK_IS_*` / `XFCE_IS_PANEL_PLUGIN` before any
  dereference, returns `TRUE` to stay installed, and does nothing on anything unexpected.
- A missing, unreadable, or malformed `colors.css` degrades to "no colours" — `GtkCssProvider`
  parsing errors are captured, never propagated.
- Every GTK/panel call site null-checks its inputs; no assumption that an ancestor walk succeeds.

**Rollback levers, in escalating order:**

1. User: `xfce4-launcher-colors --disable` removes the module from `/Gtk/Modules`; `xfce4-panel -r`
   restores the native panel. `colors.css` is preserved.
2. Package: uninstalling leaves the panel exactly as before; user config is kept (PRD §13.8).
3. Repository: revert the offending slice PR. Slices 1–2 are additive scaffolding and a throwaway
   probe; slices 3–8 each revert independently without touching the ones before them.

## Dependencies

- **Prerequisite of slice 1 (blocking):** install `meson`, `ninja`, and the dev packages for
  `gtk+-3.0`, `libxfce4panel-2.0`, `libxfce4util-1.0`, `libxfconf-0`. None are present on this
  machine. Present: `gcc 13.3.0`, `pkg-config 1.8.1`, a running `xfce4-panel 4.18.4` on Ubuntu 24.04
  — sufficient to host the H0 spike once dev packages are installed.
- xfce4-panel ≥ 4.16 at runtime; below that the module detects and warns once.
- **4.16 and 4.20 verification requires environments not available on this machine** (containers or
  VMs, wired up in the H3 CI matrix).

## Success Criteria

- [ ] `meson test -C build` passes and covers CSS generation, id/colour validation, `colors.css`
      read/write with RF-8 cleanup, and `settings.ini` handling.
- [ ] `strict_tdd` is `true` in `openspec/config.yaml` once slice 1 lands.
- [ ] Right-click a launcher on `xfce4-panel` 4.18.4 shows "Color de fondo…" above "Propiedades".
- [ ] Applying a colour paints in < 100 ms with no panel flicker (PRD §13.2).
- [ ] Colours survive logout/login; unchecking autoload restores the native panel (PRD §13.3, §13.4).
- [ ] `ps aux` shows no process of ours; `smem`/`pmap` attributes < 1 MB to the module (RNF-1).
- [ ] Removing a launcher clears its entry from `colors.css` (RF-8, PRD §13.7).
- [ ] Zero panel crashes attributable to the module across the H4 distro matrix.
- [ ] Every slice lands within the 400-line budget, or carries an accepted `size:exception`.

## Open Questions

Carried forward deliberately — these are **not** decided by this proposal.

| # | Question | Resolved by |
|---|---|---|
| 1 | Which `GtkStyleProvider` priority wins in practice — `APPLICATION` (600) or `USER` (800) — given the panel registers its own widget-level `APPLICATION` provider? | H0-b, empirical |
| 2 | Does the background visibly paint over real themes (Adwaita, Xubuntu default) without breaking hover? | H0-b, empirical |
| 3 | `map` hook timing and filtering cost in a live panel | H0-b, empirical |
| 4 | Menu-caching code path re-confirmed on 4.16.0 (verified on 4.18.0 and `master`) | H0, source read |
| 5 | Distro-patched headers versus upstream | After `libxfce4panel-2.0-dev` is installed |
| 6 | Code citations come from GitHub mirrors — re-confirm against `gitlab.xfce.org` | H0 |
