# Launcher Color Menu Specification

## Purpose

Defines the user-facing context-menu interaction for assigning, applying, and
removing a launcher's background color, and the insertion-timing constraint
imposed by `xfce4-panel`'s once-built, cached per-plugin menu.

## Requirements

### Requirement: Background Color Menu Item (RF-1)

The system MUST insert a "Color de fondo…" menu item into each detected
launcher's native right-click context menu, positioned above "Propiedades".

#### Scenario: Menu item appears on a detected launcher

- GIVEN a launcher plugin has been detected by the module
- WHEN the user right-clicks that launcher
- THEN the context menu shows "Color de fondo…" above "Propiedades"
- Manual verification only: menu render order requires a live `xfce4-panel` session.

### Requirement: Menu Item Insertion Timing

Because `xfce4-panel` builds and caches a plugin's context menu once, the
system MUST insert its custom menu items (RF-1, RF-5) at launcher-detection
time and MUST NOT insert them when the menu opens or on any later popup.

#### Scenario: Item is present on the very first right-click

- GIVEN a launcher was just detected by the module (widget mapped)
- WHEN the user right-clicks it for the first time, before any prior popup
- THEN "Color de fondo…" is already present in the menu
- Manual verification only: menu-build caching is a panel-internal timing detail.

#### Scenario: Late insertion is a prohibited path, not a fallback

- GIVEN a launcher's context menu has already been built and cached
- WHEN the module would otherwise attempt to insert an item after that point
- THEN the module MUST NOT rely on or attempt insertion at that point
- Verified by code review of the insertion call site, not by an automated test.

### Requirement: Color Selection Dialog (RF-2)

The system MUST open a GTK color-chooser dialog offering quick-palette
swatches and a free RGBA editor with alpha support when "Color de fondo…" is
activated.

#### Scenario: Dialog opens with alpha support

- GIVEN the user selects "Color de fondo…" from a launcher's menu
- WHEN the dialog opens
- THEN it offers preset swatches and a custom editor accepting an alpha channel
- Manual verification only: dialog composition requires a live display.

### Requirement: Instant Color Application (RF-3)

The system MUST apply the selected color to the launcher's background
immediately upon confirmation, without restarting `xfce4-panel`.

#### Scenario: Color applies without panel restart

- GIVEN the user has confirmed a color in the dialog
- WHEN the dialog closes
- THEN the launcher's background updates visibly with no panel process restart
- Manual verification only: visible painting cannot be asserted by unit tests.

### Requirement: Remove Color Visibility (RF-5)

The system MUST show a "Quitar color" menu item only when the launcher
currently has an assigned color, MUST implement this using
`gtk_widget_set_no_show_all()` combined with `gtk_widget_set_visible()` on the
already-inserted item, and MUST NOT use `xfce_panel_plugin_menu_destroy()` to
achieve this.

#### Scenario: Item hidden for an uncolored launcher

- GIVEN a launcher has no assigned color
- WHEN the user opens its context menu
- THEN "Quitar color" is not visible
- Manual verification only: requires a live panel.

#### Scenario: Item visible for a colored launcher

- GIVEN a launcher has an assigned color
- WHEN the user opens its context menu
- THEN "Quitar color" is visible, and activating it restores the original appearance
- Manual verification only: requires a live panel.

#### Scenario: Visibility toggling never destroys another module's items

- GIVEN another module has also inserted custom items into the same menu
- WHEN this module updates "Quitar color" visibility
- THEN the other module's items remain intact and unaffected
- Manual/integration verification only: `menu_destroy()` semantics require a live multi-module panel.
