/* lc-menu.c — see lc-menu.h.
 *
 * Hooked on "show", NOT "map": measured on the live panel (see the
 * project brief this module was built from), at "show" the menu is
 * still unrealized and unallocated (alloc_h=1), so an item appended here
 * is present for the menu's first size negotiation and it gets its full
 * natural height. At "map" the menu is already sized and GTK adds
 * scroll arrows instead of growing it. The menu is rebuilt on every
 * popup, so the per-menu "already populated" guard below is stored on
 * the menu instance itself (which is fresh every time), never assumed to
 * survive across popups.
 */
#include "lc-menu.h"

#include <glib/gi18n.h>
#include "lc-color.h"
#include "lc-dialogs.h"

/* Quick palette shown inline in the menu, so tagging several windows
 * does not mean opening the full dialog once per window. Same eight
 * colours as the approved spike, expressed as canonical "#rrggbbaa" hex
 * so they round-trip through lc_color_parse()/lc_color_to_css() exactly
 * like every other colour in the tree. Names are marked with N_() for
 * extraction and translated at build time with _(), since this is a
 * static initializer. */
typedef struct
{
  const gchar *name;
  const gchar *hex;
} LcSwatch;

static const LcSwatch LC_PALETTE[] = {
  { N_("Red"),       "#e53935d9" },
  { N_("Orange"),    "#ff8c00d9" },
  { N_("Yellow"),    "#fdd835d9" },
  { N_("Green"),     "#43a047d9" },
  { N_("Turquoise"), "#00acc1d9" },
  { N_("Blue"),      "#1e88e5d9" },
  { N_("Violet"),    "#8e44add9" },
  { N_("Pink"),      "#d81b60d9" },
};

/* Per-item closure: which button/window this menu item acts on, and
 * (for a swatch item only) which colour it applies. Freed via
 * g_object_set_data_full() alongside the GtkMenuItem it is attached to. */
typedef struct
{
  LcTasklistContext *ctx;
  GtkWidget         *button;
  gchar             *hex; /* NULL for "No colour" and "Custom…" */
} LcMenuClosure;

static void
lc_menu_closure_free (gpointer data)
{
  LcMenuClosure *closure = data;

  if (closure == NULL)
    return;

  g_free (closure->hex);
  g_free (closure);
}

/* A 16x16 solid swatch so the colour is visible in the menu itself.
 * Always rendered fully opaque, regardless of the swatch's own stored
 * alpha, matching the approved spike behaviour: the swatch is a colour
 * sample, not a preview of the exact translucency that will be applied. */
static GtkWidget *
lc_menu_swatch_image (const LcColor *color)
{
  GdkPixbuf *pixbuf;
  guint32 packed;
  GtkWidget *image;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
  packed = ((guint32) color->r << 24) | ((guint32) color->g << 16) |
           ((guint32) color->b << 8) | 0xffu;
  gdk_pixbuf_fill (pixbuf, packed);

  image = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return image;
}

/* An empty 16x16 placeholder, so "No colour" and "Custom…" keep their
 * labels aligned with the swatch rows above/below them. */
static GtkWidget *
lc_menu_swatch_pad (void)
{
  GtkWidget *pad = gtk_drawing_area_new ();

  gtk_widget_set_size_request (pad, 16, 16);

  return pad;
}

static GtkWidget *
lc_menu_item_new (GtkWidget *icon, const gchar *label_text, gboolean active)
{
  GtkWidget *item = gtk_menu_item_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

  gtk_box_pack_start (GTK_BOX (box), icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new (label_text), FALSE, FALSE, 0);
  if (active)
    gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("\xe2\x9c\x93" /* U+2713 CHECK MARK */), FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (item), box);

  return item;
}

static void
lc_menu_on_no_colour (GtkMenuItem *item, gpointer user_data)
{
  LcMenuClosure *closure = user_data;
  WnckWindow *win;

  (void) item;

  win = lc_tasklist_resolve_window (closure->button);
  if (win == NULL)
    return;

  lc_winstore_unset (lc_tasklist_context_store (closure->ctx), (gulong) wnck_window_get_xid (win));
  lc_tasklist_apply_change (closure->ctx, closure->button);
}

static void
lc_menu_on_swatch (GtkMenuItem *item, gpointer user_data)
{
  LcMenuClosure *closure = user_data;
  WnckWindow *win;
  LcColor color;

  (void) item;

  if (!lc_color_parse (closure->hex, &color))
    return;

  win = lc_tasklist_resolve_window (closure->button);
  if (win == NULL)
    return;

  lc_winstore_set (lc_tasklist_context_store (closure->ctx),
                    (gulong) wnck_window_get_xid (win), &color);
  lc_tasklist_apply_change (closure->ctx, closure->button);
}

static void
lc_menu_on_custom (GtkMenuItem *item, gpointer user_data)
{
  LcMenuClosure *closure = user_data;

  (void) item;

  lc_dialogs_open_for_button (closure->ctx, closure->button);
}

/* Determines the currently applied colour for `button`'s window, if any,
 * so the submenu can tick the right entry. Best-effort: an unresolved
 * window (e.g. it closed between right-click and this callback) simply
 * draws the submenu with no tick, which is still fully functional. */
static gboolean
lc_menu_current_color (LcTasklistContext *ctx, GtkWidget *button, LcColor *out)
{
  WnckWindow *win = lc_tasklist_resolve_window (button);

  if (win == NULL)
    return FALSE;

  return lc_winstore_get (lc_tasklist_context_store (ctx), (gulong) wnck_window_get_xid (win), out);
}

static gboolean
lc_color_equal (const LcColor *a, const LcColor *b)
{
  return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
}

static GtkWidget *
lc_menu_build_submenu (LcTasklistContext *ctx, GtkWidget *button)
{
  GtkWidget *submenu = gtk_menu_new ();
  LcColor current;
  gboolean has_current = lc_menu_current_color (ctx, button, &current);
  guint i;

  /* "No colour": first, and ALWAYS present. */
  {
    GtkWidget *item = lc_menu_item_new (lc_menu_swatch_pad (), _("No colour"), !has_current);
    LcMenuClosure *closure = g_new0 (LcMenuClosure, 1);

    closure->ctx = ctx;
    closure->button = button;
    g_object_set_data_full (G_OBJECT (item), "lc-closure", closure, lc_menu_closure_free);
    g_signal_connect (item, "activate", G_CALLBACK (lc_menu_on_no_colour), closure);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  }

  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), gtk_separator_menu_item_new ());

  /* Eight quick swatches. */
  for (i = 0; i < G_N_ELEMENTS (LC_PALETTE); i++)
    {
      LcColor swatch_color;
      gboolean active;
      GtkWidget *item;
      LcMenuClosure *closure;

      if (!lc_color_parse (LC_PALETTE[i].hex, &swatch_color))
        continue; /* unreachable for the fixed table above, guarded anyway */

      active = has_current && lc_color_equal (&current, &swatch_color);
      item = lc_menu_item_new (lc_menu_swatch_image (&swatch_color), _(LC_PALETTE[i].name), active);

      closure = g_new0 (LcMenuClosure, 1);
      closure->ctx = ctx;
      closure->button = button;
      closure->hex = g_strdup (LC_PALETTE[i].hex);
      g_object_set_data_full (G_OBJECT (item), "lc-closure", closure, lc_menu_closure_free);
      g_signal_connect (item, "activate", G_CALLBACK (lc_menu_on_swatch), closure);

      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }

  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), gtk_separator_menu_item_new ());

  /* "Custom…": opens the full dialog. */
  {
    GtkWidget *item = gtk_menu_item_new_with_label (_("Custom…"));
    LcMenuClosure *closure = g_new0 (LcMenuClosure, 1);

    closure->ctx = ctx;
    closure->button = button;
    g_object_set_data_full (G_OBJECT (item), "lc-closure", closure, lc_menu_closure_free);
    g_signal_connect (item, "activate", G_CALLBACK (lc_menu_on_custom), closure);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  }

  gtk_widget_show_all (submenu);

  return submenu;
}

static void
lc_menu_populate (GtkMenu *menu, LcTasklistContext *ctx, GtkWidget *button)
{
  GtkWidget *sep = gtk_separator_menu_item_new ();
  GtkWidget *top = gtk_menu_item_new_with_label (_("Background colour"));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (top), lc_menu_build_submenu (ctx, button));

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), sep);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), top);
  gtk_widget_show (sep);
  gtk_widget_show (top);
}

static gboolean
lc_menu_show_hook (GSignalInvocationHint *hint, guint n_params, const GValue *params,
                    gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (g_value_get_object (&params[0]));
  LcTasklistContext *ctx = user_data;
  GtkWidget *attach;

  (void) hint;
  (void) n_params;

  if (!GTK_IS_MENU (widget))
    return TRUE;

  /* Filtering on the attach widget being a real tasklist window button
   * (not merely "any XfceArrowButton") also excludes the tasklist's own
   * overflow arrow and, incidentally, still excludes the "move to
   * another workspace" submenu, whose attach widget is never an
   * XfceArrowButton at all. */
  attach = gtk_menu_get_attach_widget (GTK_MENU (widget));
  if (!lc_tasklist_is_window_button (attach))
    return TRUE;

  /* The menu is rebuilt on every popup, so this guard lives on the menu
   * instance itself (fresh every time) and only protects against "show"
   * firing more than once for the SAME instance (e.g. a reposition). */
  if (g_object_get_data (G_OBJECT (widget), "lc-menu-done") != NULL)
    return TRUE;
  g_object_set_data (G_OBJECT (widget), "lc-menu-done", GINT_TO_POINTER (1));

  lc_menu_populate (GTK_MENU (widget), ctx, attach);

  return TRUE;
}

gulong
lc_menu_install_hook (LcTasklistContext *ctx)
{
  g_return_val_if_fail (ctx != NULL, 0);

  return g_signal_add_emission_hook (g_signal_lookup ("show", GTK_TYPE_WIDGET), 0,
                                      lc_menu_show_hook, ctx, NULL);
}

void
lc_menu_uninstall_hook (gulong hook_id)
{
  if (hook_id == 0)
    return;

  g_signal_remove_emission_hook (g_signal_lookup ("show", GTK_TYPE_WIDGET), hook_id);
}
