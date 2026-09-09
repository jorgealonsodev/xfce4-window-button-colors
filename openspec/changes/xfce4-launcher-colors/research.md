# Research — xfce4-launcher-colors

**Change:** `xfce4-launcher-colors`
**Phase:** research (lane selected by the user; completion was mandatory before proposal)
**Date:** 2026-09-09
**Status:** complete — all four lanes answered; every load-bearing claim independently re-verified by the orchestrator against upstream source.

## Scope

Four lanes left open by `explore.md`:

1. The `xfce_panel_plugin_menu_insert_item()` contract and its availability on 4.16 / 4.18 / 4.20.
2. Whether inserted menu items persist or must be re-inserted per popup.
3. The real CSS node structure of a launcher and the provider priority needed to beat a theme.
4. The most reliable widget-creation hook for a module loaded before any launcher exists.

## Lane 1 — menu API contract: VERIFIED, no version risk

Both functions are declared verbatim in the `xfce4-panel-4.16.0` tagged header,
`libxfce4panel/xfce-panel-plugin.h` (lines 205 and 212):

```c
void  xfce_panel_plugin_menu_insert_item (XfcePanelPlugin *plugin, GtkMenuItem *item);
void  xfce_panel_plugin_menu_destroy     (XfcePanelPlugin *plugin);
```

`xfce_panel_plugin_get_unique_id()` is declared at line 166 and returns `gint`.

Neither function carries a `Since:` annotation in the header or in the reference manual, which means
they predate 4.16 rather than having been added in it. The risk this lane was opened to test — "the
API might be a post-4.16 addition, which would break RNF-7" — **does not materialise**.

Reference manual text: *"Insert a custom menu item to the plugin's right click menu. This item is
packed below the first item in the menu, which displays the plugin's name."*

Production call site, `xfce4-whiskermenu-plugin` (`Plugin::Plugin` constructor), confirms the call
is made once at plugin construction time, not per popup.

Sources:
- https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/xfce4-panel-4.16.0/libxfce4panel/xfce-panel-plugin.h
- https://developer.xfce.org/xfce4-panel/XfcePanelPlugin.html
- https://github.com/xfce-mirror/xfce4-whiskermenu-plugin/blob/main/panel-plugin/plugin.cpp

## Lane 2 — menu item lifetime: more restrictive than first reported

`xfce_panel_plugin_menu_insert_item()` only appends to an internal list:

```c
plugin->priv->menu_items = g_slist_append (plugin->priv->menu_items,
                                           g_object_ref_sink (item));
```

The items reach a live `GtkMenu` only in `xfce_panel_plugin_menu_get()`, and — this is the decisive
detail — the loop that appends them sits **inside** the lazy-construction branch
(`xfce-panel-plugin.c`, 4.18.0, lines 1193 and 1207):

```c
if (G_UNLIKELY (plugin->priv->menu == NULL))
  {
    ...
    /* add custom menu items */
    for (li = plugin->priv->menu_items; li != NULL; li = li->next)
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (li->data));
    ...
  }
```

The right-click handler calls `xfce_panel_plugin_menu_get()` on every press, but after the first
build that call returns the cached `plugin->priv->menu` untouched.

**Correction to the research agent's own conclusion.** The agent reported that items simply
"persist for the plugin widget's lifetime". That is true but incomplete, and the omitted half is the
part that constrains the design:

- Inserting an item **after** the menu has been built has no visible effect at all.
- `xfce_panel_plugin_menu_destroy()` is not a usable escape hatch. It frees **every** custom item on
  that plugin, not only ours, and it is a no-op while the menu is visible:

  ```c
  if (plugin->priv->menu != NULL && !gtk_widget_get_visible (GTK_WIDGET (plugin->priv->menu)))
    {
      g_slist_free_full (plugin->priv->menu_items, g_object_unref);
      plugin->priv->menu_items = NULL;
      gtk_menu_detach (GTK_MENU (plugin->priv->menu));
      plugin->priv->menu = NULL;
    }
  ```

**Design consequences:** insert both items as soon as the launcher widget is detected, before the
user's first right-click; never call `menu_destroy()` to refresh our own item; implement RF-5's
conditional visibility with `gtk_widget_set_no_show_all()` plus `gtk_widget_set_visible()` on the
already-inserted item.

Source: https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/xfce4-panel-4.18.0/libxfce4panel/xfce-panel-plugin.c

## Lane 3 — CSS node structure: an upstream naming bug, verified across three tags

`launcher_plugin_init()` in `plugins/launcher/launcher.c` names the clickable button twice, and the
second call targets the wrong variable:

```c
plugin->button = xfce_panel_create_button ();
gtk_widget_set_name (plugin->button, "launcher-button");     /* 4.18.0 line 350 */
...
plugin->arrow = xfce_arrow_button_new (GTK_ARROW_UP);        /* 4.18.0 line 391 */
gtk_button_set_relief (GTK_BUTTON (plugin->arrow), GTK_RELIEF_NONE);
gtk_widget_set_name (plugin->button, "launcher-arrow");      /* 4.18.0 line 395 — button, not arrow */
```

`gtk_widget_set_name()` overwrites, so the clickable button's effective runtime name is
**`launcher-arrow`**. A grep for `plugin->arrow` across the file confirms the arrow widget is never
named at all.

Independently re-verified by the orchestrator at three tags — the pattern is identical in
`xfce4-panel-4.16.0` (lines 364 / 399), `xfce4-panel-4.18.0` (350 / 395) and `master` (373 / 418).

Corroboration from the panel's own code: its internal minimum-size rule uses the selector
`#launcher-arrow` applied to `plugin->button`'s style context:

```c
css_string = g_strdup_printf ("#launcher-arrow { min-height: 0; min-width: 0; }");
gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (css_provider),
                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
```

**This contradicts `explore.md`, which assumed a `#launcher-button` selector.** A rule written
against `#launcher-button` matches nothing and fails silently — the worst possible failure shape for
this feature. It is an upstream bug, but since it is identical from 4.16.0 to `master` it is a
stable contract in practice.

Because `launcher-arrow` is shared by every launcher instance, per-instance targeting comes from the
unique outer container: `#launcher-<id> #launcher-arrow`.

Provider priorities (GTK3): `THEME` = 200, `APPLICATION` = 600, `USER` = 800.

**Open decision, deferred to the H0 spike.** The panel already registers an `APPLICATION`-priority
provider on the widget's own style context. Widget-level providers outrank screen-level ones at
equal priority, so a screen-level `APPLICATION` provider of ours may lose. `USER` would win but
ties with the user's own `gtk.css`, taking away their last word. This is an empirical question and
should not be settled on paper.

Sources:
- https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/xfce4-panel-4.16.0/plugins/launcher/launcher.c
- https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/xfce4-panel-4.18.0/plugins/launcher/launcher.c
- https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/master/plugins/launcher/launcher.c
- https://docs.gtk.org/gtk3/const.STYLE_PROVIDER_PRIORITY_THEME.html
- https://docs.gtk.org/gtk3/const.STYLE_PROVIDER_PRIORITY_APPLICATION.html

## Lane 4 — widget-creation hook: emission hook on `GtkWidget::map`

GObject documents `g_signal_add_emission_hook()` as global: *"Adds an emission hook for a signal,
which will get called for any emission of that signal, independent of the instance."* It requires
only that the signal be registered, which happens when `GTK_TYPE_WIDGET`'s class is first
referenced at `gtk_init()` time — long before any launcher plugin exists.

A module can therefore call `g_signal_lookup("map", GTK_TYPE_WIDGET)` once in its init function and
register a single hook. Setup is O(1), the hook fires only on real map events, and nothing polls.
This satisfies RNF-2's zero-idle-CPU requirement. `gtk_window_list_toplevels()` polling is
disqualified by that same requirement.

`map` is preferred over `realize`: it guarantees the widget is about to become visible and stable in
the hierarchy, which is when walking ancestors to find the `XfcePanelPlugin` is reliable. `realize`
fires earlier and can occur repeatedly across reparenting.

**Failure mode to design against:** the hook fires for *every* mapped widget in the process, so the
callback must filter cheaply — check the widget name prefix or `XFCE_IS_PANEL_PLUGIN` ancestry —
before doing any real work.

**VERIFIED (negative result):** a targeted search of `panel/panel-application.c` surfaced no
application-wide `g_signal_new()` plugin-insertion signal. `panel_application_plugin_insert()`
creates the widget and calls `panel_itembar_insert()` without emitting one. The existing
`"provider-signal"` connection is per-plugin and fires only after the instance exists, so it is not
a creation-detection hook. Treat this as "no such signal was surfaced by a targeted search" rather
than an exhaustive proof of absence.

**UNCONFIRMED:** whether `appmenu-gtk-module` itself uses this exact technique. Its source could not
be retrieved. The `/Gtk/Modules` autoload precedent stands (established in exploration); the
emission-hook parallel is plausible but unverified and must not be cited as fact.

Sources:
- https://docs.gtk.org/gobject/func.signal_add_emission_hook.html
- https://raw.githubusercontent.com/xfce-mirror/xfce4-panel/master/panel/panel-application.c

## Consequences applied to the PRD (v1.1)

| Finding | PRD section changed |
|---|---|
| Public menu API replaces signal interception | §7.3 diagram, §7.3 key points, §11 |
| Effective selector is `#launcher-<id> #launcher-arrow` | §7.3 example CSS, §7.4, §11 (new risk row) |
| Menu is built once and cached | §5 RF-5 note, §7.4, §11 (new risk row) |
| No upstream evidence for a GTK4 "4.22+" date | §11 |
| A vs. B decided on documentary evidence; spike rescoped | §7.4, §12 H0 (2–3 → 3–4 days) |

## Residual gaps

1. Provider priority and whether the background actually paints over real themes — H0, empirical.
2. `map` hook timing and filtering cost in a live panel — H0, empirical.
3. Menu-caching path re-confirmed on 4.16.0 (verified on 4.18.0 and `master`).
4. Distro-patched headers versus upstream — check after installing `libxfce4panel-2.0-dev`.
5. Code citations come from GitHub mirrors; re-confirm against `gitlab.xfce.org`.
