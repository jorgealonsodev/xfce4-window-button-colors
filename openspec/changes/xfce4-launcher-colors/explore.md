# Exploration — xfce4-launcher-colors

**Change:** `xfce4-launcher-colors`
**Phase:** explore
**Date:** 2026-09-09
**Engram topic key:** `sdd/xfce4-launcher-colors/explore` (observation #2511)
**Question:** Is PRD option A (in-process GTK module loaded by `xfce4-panel`) technically sound, or does evidence force Plan B (a new panel plugin)?

## Method and provenance caveat

The repository is greenfield: no source, no `meson.build`. The local toolchain lacks `meson`, `ninja` and every XFCE/GTK3 development header, so nothing could be compiled or inspected locally. All findings below come from upstream XFCE sources reached over the network: GitHub mirrors of `gitlab.xfce.org` (`xfce-mirror/xfce4-panel`, `raboof/xfce4-panel`), `developer.xfce.org` and `docs.xfce.org`.

`gitlab.xfce.org` could not be rendered directly — its JavaScript file browser returns only a loading placeholder — so code-level citations point at mirrors rather than the canonical instance. This is a provenance caveat, not a correctness caveat, but every code-level claim below should be re-confirmed against `gitlab.xfce.org` once local development headers are installed.

## Findings against PRD section 7.4

### 1. Launcher widget naming — VERIFIED, the PRD is correct

`libxfce4panel/xfce-panel-plugin.c`, in `xfce_panel_plugin_set_property()`, builds the widget name for every plugin:

```c
name = g_strdup_printf ("%s-%d", priv->name, priv->unique_id);
gtk_widget_set_name (GTK_WIDGET (object), name);
```

This confirms the `launcher-13` naming shape the PRD depends on. `xfce_panel_plugin_get_unique_id()` exists and returns `gint`.

Most importantly, this resolves the highest-risk item in the PRD favourably. `plugins/launcher/launcher.desktop.in` ships `X-XFCE-Internal=TRUE`, and `panel/panel-module.c` runs a plugin internally — with no wrapper process — whenever `X-XFCE-Internal=TRUE`, the session is X11, and external execution is not forced. There are no per-plugin hardcoded exceptions. Launchers therefore run inside the `xfce4-panel` process on X11, so an in-process GTK module does see their live widgets.

**Caveat:** the in-process guarantee found in the code is X11-specific. Wayland behaviour is unconfirmed, which is consistent with the PRD's decision not to actively test Wayland in v1.

### 2. Module autoload via XSETTINGS `/Gtk/Modules` — VERIFIED

The precedent the PRD cites is real. `appmenu-gtk-module` registers itself with:

```sh
xfconf-query -c xsettings -p /Gtk/Modules -n -t string -s "..."
```

`xfsettingsd/xsettings.c` forwards any `/Gtk/*` key generically to XSETTINGS; no allowlist excludes `Modules`. GTK3's own documentation confirms that the `gtk-modules` `GtkSettings` property, populated from XSETTINGS, is one of three standard module-loading mechanisms, alongside the `GTK3_MODULES` environment variable and `--gtk-module`.

### 3. Menu hook point — the PRD's proposed mechanism is wrong, and there is a better answer

The PRD's candidates — intercepting `popup-menu` or `button-press-event` — are not the right tool. libxfce4panel already exposes a public, documented, stable API for exactly this purpose:

- `xfce_panel_plugin_menu_insert_item (XfcePanelPlugin *plugin, GtkMenuItem *item)`
- `xfce_panel_plugin_menu_destroy (XfcePanelPlugin *plugin)`

It is used in production by `xfce4-whiskermenu-plugin`. Because the widget carrying the `launcher-<id>` name **is** the `XfcePanelPlugin` GObject itself, a module can enumerate `gtk_window_list_toplevels()`, locate widgets by name, cast, and call this API directly. That is substantially more stable across 4.16 / 4.18 / 4.20 than signal interception.

**Gap the PRD glosses over:** at module-load time, no launcher widgets exist yet. The module needs a widget-creation hook — a `realize`/`map` signal or a GObject emission hook — to catch each launcher instance as it appears. This is real, currently unscoped engineering work, not a known-solved problem.

**ASSUMED, needs a build spike:** whether one `xfce_panel_plugin_menu_insert_item()` call per widget instance is sufficient, or whether the item must be re-inserted per menu construction.

No evidence found forces Plan B.

### 4. CSS scoping — the PRD's `colors.css` shape needs correction

`docs.xfce.org` confirms two distinct name levels exist:

- a per-instance `launcher-<id>` name on the outer container (confirmed in finding 1), and
- a separate, generic, **shared** `#launcher-button` name on the inner clickable widget that actually renders and handles hover.

The PRD's example rule — `#launcher-13 { background-color: ...; }` alone — may not visibly paint, because the inner button paints its own background over it. The correct selector is likely compound or descendant, for example `#launcher-13 #launcher-button { ... }`, rather than a bare outer-id rule.

### 5. GTK4 horizon — the PRD's risk-table date is unevidenced

As of xfce4-panel 4.20.x the panel is still GTK 3.24. The actual roadmap priority for 4.20 was Wayland and Layer-Shell support, not GTK4. Commentary found explicitly states that GTK4 transition timing is undecided. No source confirms "4.22+" as a real target. The date claim in the PRD risk table should be softened or removed.

### 6. Testability

The stance already recorded in `openspec/config.yaml` holds.

Unit-testable with Meson, as pure logic isolated from GTK and panel glue:

- CSS generation
- hex and RGBA parsing and validation
- `colors.css` read, write and stale-entry cleanup (RF-8)
- xfconf key logic behind a thin interface

Requires manual or integration verification against a live panel:

- module loading via `/Gtk/Modules`
- the widget hook actually finding real plugin instances
- visual confirmation that the CSS beats the active theme
- cross-version behaviour on 4.16 / 4.18 / 4.20

## Risks, ranked by likelihood of breaking the elected architecture

| # | Risk | Level | Note |
|---|------|-------|------|
| 1 | Menu-hook engineering gap (finding 3) | Medium-high | The right API exists, but "detect new launcher widget, insert once" is unscoped. Possibly tight for the PRD's 2–3 day H0 estimate. |
| 2 | CSS selector shape (finding 4) | Medium | Risk of a visibly broken first demo if `colors.css` targets only the outer container. |
| 3 | Widget naming and internal-launcher assumption (finding 1) | Low | Strongly verified across independent sources. |
| 4 | XSETTINGS autoload (finding 2) | Low | Strongly verified via a shipped precedent plus official GTK documentation. |
| 5 | GTK4 migration timeline (finding 5) | Low-medium | Risk of planning around an unevidenced date. |
| 6 | Citation provenance (mirrors, not canonical GitLab) | Low | Re-confirm once local dev headers are installed. |

## Conclusion

**Ready for proposal: yes.**

Two of the three gating assumptions are strongly verified as PRD-correct. The third has a stronger answer than the PRD proposed — a public API plus a widget-creation hook — which should be folded into the proposal and design rather than treated as a blocker. No evidence found forces Plan B.

## Open questions carried into research

1. Confirm `xfce_panel_plugin_menu_insert_item()` signature, availability and semantics against canonical `gitlab.xfce.org` sources for 4.16, 4.18 and 4.20.
2. Determine whether menu items must be re-inserted per menu construction or persist for the widget's lifetime.
3. Confirm the exact inner-widget CSS name and node structure, and the provider priority needed to beat common themes.
4. Identify the most reliable widget-creation hook for catching launcher instances after module load.
