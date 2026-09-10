#include <string.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

/* End-to-end spike for the tasklist pivot:
 *   right-click a window button -> "Color de fondo..." -> chooser -> live paint,
 *   keyed on WM_CLASS so every window of that application is painted. */

static GtkCssProvider *provider = NULL;
static GHashTable *colors = NULL;          /* wm_class -> "rgba(...)" */

/* Resolve a tasklist button to its window's WM_CLASS via its title. */
static gchar *
wm_class_for_button (GtkWidget *button)
{
  gchar *tip = gtk_widget_get_tooltip_text (button);
  WnckScreen *scr;
  GList *l;
  gchar *out = NULL;

  if (tip == NULL)
    return NULL;

  scr = wnck_screen_get_default ();
  wnck_screen_force_update (scr);
  for (l = wnck_screen_get_windows (scr); l != NULL && out == NULL; l = l->next)
    {
      WnckWindow *win = l->data;
      if (g_strcmp0 (wnck_window_get_name (win), tip) == 0)
        out = g_strdup_printf ("%lu", (gulong) wnck_window_get_xid (win));
    }
  g_free (tip);
  return out;
}

static gchar *
css_class_for (const gchar *wm_class)
{
  gchar *s = g_strdup_printf ("lc-win-%s", wm_class);
  for (gchar *p = s; *p; p++)
    if (!g_ascii_isalnum (*p)) *p = '-';
  return g_ascii_strdown (s, -1);
}

static void
rebuild_css (void)
{
  GString *out = g_string_new (NULL);
  GHashTableIter it;
  gpointer k, v;
  g_hash_table_iter_init (&it, colors);
  while (g_hash_table_iter_next (&it, &k, &v))
    {
      gchar *cls = css_class_for (k);
      g_string_append_printf (out,
        ".%s { background-image: none; background-color: %s; border-radius: 4px; }\n",
        cls, (const gchar *) v);
      g_free (cls);
    }
  gtk_css_provider_load_from_data (provider, out->str, -1, NULL);
  g_message ("FULL css now:\n%s", out->str);
  g_string_free (out, TRUE);
}

/* Tag every currently-mapped tasklist button whose class has a colour. */
static void
retag (GtkWidget *root)
{
  if (GTK_IS_CONTAINER (root))
    {
      GList *ch = gtk_container_get_children (GTK_CONTAINER (root)), *l;
      for (l = ch; l; l = l->next) retag (GTK_WIDGET (l->data));
      g_list_free (ch);
    }
  {
    GtkWidget *par = gtk_widget_get_parent (root);
    if (par && g_strcmp0 (G_OBJECT_TYPE_NAME (par), "XfceTasklist") == 0
            && g_strcmp0 (G_OBJECT_TYPE_NAME (root), "XfceArrowButton") == 0)
      {
        GtkStyleContext *ctx = gtk_widget_get_style_context (root);
        gchar *wc = wm_class_for_button (root);
        GList *old = gtk_style_context_list_classes (ctx), *o;
        for (o = old; o; o = o->next)
          if (g_str_has_prefix ((const gchar *) o->data, "lc-win-"))
            gtk_style_context_remove_class (ctx, (const gchar *) o->data);
        g_list_free (old);
        if (wc && g_hash_table_contains (colors, wc))
          {
            gchar *cls = css_class_for (wc);
            gtk_style_context_add_class (ctx, cls);
            g_free (cls);
          }
        g_free (wc);
      }
  }
}

static void retag (GtkWidget *root);
static void retag_all (void);

/* Quick palette shown inline in the menu, so tagging several windows does
 * not mean opening a dialog once per window. The full GtkColorChooser
 * stays available behind "Personalizado...". */
typedef struct { const gchar *name; const gchar *rgba; } LcSwatch;
static const LcSwatch PALETTE[] = {
  { "Rojo",     "rgba(229,57,53,0.85)"  },
  { "Naranja",  "rgba(255,140,0,0.85)"  },
  { "Amarillo", "rgba(253,216,53,0.85)" },
  { "Verde",    "rgba(67,160,71,0.85)"  },
  { "Turquesa", "rgba(0,172,193,0.85)"  },
  { "Azul",     "rgba(30,136,229,0.85)" },
  { "Violeta",  "rgba(142,68,173,0.85)" },
  { "Rosa",     "rgba(216,27,96,0.85)"  },
};

/* A 16x16 solid swatch so the colour is visible in the menu itself. */
static GtkWidget *
swatch_image (const gchar *rgba)
{
  GdkRGBA c;
  GdkPixbuf *pb;
  guint32 packed;
  if (!gdk_rgba_parse (&c, rgba))
    return NULL;
  pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
  packed = ((guint32)(c.red   * 255 + 0.5) << 24)
         | ((guint32)(c.green * 255 + 0.5) << 16)
         | ((guint32)(c.blue  * 255 + 0.5) << 8)
         | 0xff;
  gdk_pixbuf_fill (pb, packed);
  {
    GtkWidget *img = gtk_image_new_from_pixbuf (pb);
    g_object_unref (pb);
    return img;
  }
}

static GtkWidget *
swatch_item (const LcSwatch *sw, gboolean active)
{
  GtkWidget *item = gtk_menu_item_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *img = sw->rgba ? swatch_image (sw->rgba) : NULL;

  if (img != NULL)
    gtk_box_pack_start (GTK_BOX (box), img, FALSE, FALSE, 0);
  else
    {
      /* "Sin color": an empty 16x16 placeholder keeps the labels aligned
       * with the swatch rows above it. */
      GtkWidget *pad = gtk_drawing_area_new ();
      gtk_widget_set_size_request (pad, 16, 16);
      gtk_box_pack_start (GTK_BOX (box), pad, FALSE, FALSE, 0);
    }
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new (sw->name), FALSE, FALSE, 0);
  if (active)
    gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("\xe2\x9c\x93"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (item), box);
  return item;
}

static void
on_swatch (GtkMenuItem *item, gpointer data)
{
  GtkWidget *button = data;
  const gchar *rgba = g_object_get_data (G_OBJECT (item), "lc-rgba");
  gchar *key = wm_class_for_button (button);
  if (key != NULL && *key != '\0')
    {
      if (rgba == NULL)
        {
          g_hash_table_remove (colors, key);
          g_message ("FULL cleared colour for window %s", key);
        }
      else
        {
          g_hash_table_insert (colors, g_strdup (key), g_strdup (rgba));
          g_message ("FULL swatch %s -> window %s", rgba, key);
        }
      rebuild_css ();
      retag_all ();
    }
  g_free (key);
}


/* Screen colour picker. GTK3 has no eyedropper -- GTK4 added one on top of a
 * desktop portal -- so this grabs the pointer, waits for one click, and reads
 * the pixel under it straight from the root window.
 *
 * The nested main loop below is a spike shortcut. In the real module this
 * must not run inside the panel process this way: see the note on
 * gtk_dialog_run(). */
typedef struct { gboolean done; gboolean ok; gint x; gint y; } LcPick;

static gboolean
on_pick_press (GtkWidget *w, GdkEventButton *ev, gpointer data)
{
  LcPick *p = data;
  (void) w;
  p->x = (gint) ev->x_root;
  p->y = (gint) ev->y_root;
  p->ok = (ev->button == 1);
  p->done = TRUE;
  return TRUE;
}

static gboolean
pick_from_screen (GtkWidget *anchor, GdkRGBA *out)
{
  GdkDisplay *dpy = gtk_widget_get_display (anchor);
  GdkSeat *seat = gdk_display_get_default_seat (dpy);
  GdkWindow *gw = gtk_widget_get_window (anchor);
  GdkCursor *cur = gdk_cursor_new_from_name (dpy, "crosshair");
  LcPick p = { FALSE, FALSE, 0, 0 };
  gulong handler;
  gboolean got = FALSE;

  if (gw == NULL)
    return FALSE;

  gtk_widget_add_events (anchor, GDK_BUTTON_PRESS_MASK);
  handler = g_signal_connect (anchor, "button-press-event",
                              G_CALLBACK (on_pick_press), &p);

  if (gdk_seat_grab (seat, gw, GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE,
                     cur, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    {
      g_signal_handler_disconnect (anchor, handler);
      if (cur) g_object_unref (cur);
      g_warning ("xfce4-launcher-colors: could not grab the pointer to pick a colour");
      return FALSE;
    }

  while (!p.done)
    gtk_main_iteration ();

  gdk_seat_ungrab (seat);
  g_signal_handler_disconnect (anchor, handler);
  if (cur) g_object_unref (cur);

  if (p.ok)
    {
      GdkWindow *root = gdk_get_default_root_window ();
      GdkPixbuf *pb = gdk_pixbuf_get_from_window (root, p.x, p.y, 1, 1);
      if (pb != NULL)
        {
          guchar *px = gdk_pixbuf_get_pixels (pb);
          out->red = px[0] / 255.0;
          out->green = px[1] / 255.0;
          out->blue = px[2] / 255.0;
          out->alpha = 0.85;   /* keep the tag translucent by default */
          g_message ("FULL picked %d,%d,%d at %d,%d", px[0], px[1], px[2], p.x, p.y);
          g_object_unref (pb);
          got = TRUE;
        }
      else
        g_warning ("xfce4-launcher-colors: could not read the pixel at %d,%d", p.x, p.y);
    }
  return got;
}


/* Live preview: paint the confirming button with the colour being chosen,
 * so the decision is shown where it is taken. */
static void
update_ok_preview (GObject *chooser, GParamSpec *ps, gpointer data)
{
  GtkWidget *ok = data;
  GtkCssProvider *prov = g_object_get_data (G_OBJECT (ok), "lc-preview-prov");
  GdkRGBA c;
  gchar a[G_ASCII_DTOSTR_BUF_SIZE];
  gdouble lum;
  gchar *css;
  (void) ps;

  if (prov == NULL)
    return;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (chooser), &c);

  /* Locale-independent, for the same reason lc_color_to_css uses it: "%.2f"
   * would emit "0,85" under a comma-decimal locale and GTK would reject the
   * whole rule. */
  g_ascii_formatd (a, sizeof a, "%.2f", c.alpha);

  /* Relative luminance decides the label colour, so the text stays readable
   * on both a pale yellow and a dark navy. */
  lum = 0.2126 * c.red + 0.7152 * c.green + 0.0722 * c.blue;

  css = g_strdup_printf (
    "button.lc-preview {"
    "  background-image: none;"
    "  background-color: rgba(%d,%d,%d,%s);"
    "  color: %s;"
    "  text-shadow: none;"
    "}",
    (int) (c.red * 255 + 0.5), (int) (c.green * 255 + 0.5), (int) (c.blue * 255 + 0.5), a,
    lum > 0.55 ? "#000000" : "#ffffff");

  gtk_css_provider_load_from_data (prov, css, -1, NULL);
  g_free (css);
}

static void
retag_all (void)
{
  GList *tops = gtk_window_list_toplevels (), *l;
  for (l = tops; l; l = l->next)
    retag (GTK_WIDGET (l->data));
  g_list_free (tops);
}

/* RF-5: remove the colour from this window. */
static void
on_clear (GtkMenuItem *item, gpointer data)
{
  GtkWidget *button = data;
  gchar *key = wm_class_for_button (button);
  (void) item;
  if (key != NULL && g_hash_table_remove (colors, key))
    {
      g_message ("FULL cleared colour for window %s", key);
      rebuild_css ();
      retag_all ();
    }
  g_free (key);
}

static void
on_choose (GtkMenuItem *item, gpointer data)
{
  GtkWidget *button = data;
  gchar *wc = wm_class_for_button (button);
  GtkWidget *dlg;
  (void) item;

  if (wc == NULL || *wc == '\0')
    { g_message ("FULL could not resolve window"); g_free (wc); return; }

  g_message ("FULL chooser for window xid=%s", wc);
  dlg = gtk_color_chooser_dialog_new ("Color de fondo", NULL);
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (dlg), TRUE);

  /* NOTE: do NOT call gtk_color_chooser_add_palette() here. Its first call
   * has the documented side effect of REMOVING the default palette, so
   * seeding our eight colours replaced GTK's much richer grid instead of
   * complementing it. The submenu already provides the quick palette; this
   * dialog's job is the full range. */

  /* Removing the colour must be reachable from here too: cancelling and
   * reopening the submenu to pick "Sin color" is a dead end for the user
   * who only realises inside the dialog. */
  gtk_dialog_add_button (GTK_DIALOG (dlg), "Sin color", GTK_RESPONSE_REJECT);
  /* Eyedropper as an icon button, the way every other colour tool shows it.
   * "color-select-symbolic" is the name GTK's own picker uses; fall back
   * through the other themed names and finally to a text label, so a theme
   * without any of them still gets a usable button rather than a blank one. */
  {
    static const gchar *ICONS[] = { "color-select-symbolic", "gtk-color-picker",
                                    "color-picker", "color-select", NULL };
    GtkIconTheme *theme = gtk_icon_theme_get_default ();
    const gchar *chosen = NULL;
    GtkWidget *btn;
    gsize i;

    for (i = 0; ICONS[i] != NULL && chosen == NULL; i++)
      if (gtk_icon_theme_has_icon (theme, ICONS[i]))
        chosen = ICONS[i];

    if (chosen != NULL)
      {
        btn = gtk_button_new ();
        gtk_button_set_image (GTK_BUTTON (btn),
          gtk_image_new_from_icon_name (chosen, GTK_ICON_SIZE_BUTTON));
        gtk_button_set_always_show_image (GTK_BUTTON (btn), TRUE);
      }
    else
      btn = gtk_button_new_with_label ("Cuentagotas");

    /* The icon alone is not self-explanatory to everyone, so keep the name
     * reachable as a tooltip. */
    gtk_widget_set_tooltip_text (btn, "Seleccionar un color de la pantalla");
    gtk_dialog_add_action_widget (GTK_DIALOG (dlg), btn, 42);
    gtk_widget_show_all (btn);
    g_message ("FULL eyedropper icon: %s", chosen ? chosen : "(none, text fallback)");

    /* Button order and emphasis. Appending left "Seleccionar" stranded in
     * the middle with no visual weight. The confirming action must be last
     * and must read as the primary one; the two auxiliary actions belong in
     * the secondary group on the left, away from the commit button. */
    {
      GtkWidget *ok = gtk_dialog_get_widget_for_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);
      GtkWidget *cancel = gtk_dialog_get_widget_for_response (GTK_DIALOG (dlg), GTK_RESPONSE_CANCEL);
      GtkWidget *none = gtk_dialog_get_widget_for_response (GTK_DIALOG (dlg), GTK_RESPONSE_REJECT);
      GtkWidget *area = ok ? gtk_widget_get_parent (ok) : NULL;

      if (GTK_IS_BUTTON_BOX (area))
        {
          if (none) gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (area), none, TRUE);
          gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (area), btn, TRUE);
        }
      if (GTK_IS_BOX (area))
        {
          if (cancel) gtk_box_reorder_child (GTK_BOX (area), cancel, -1);
          gtk_box_reorder_child (GTK_BOX (area), ok, -1);   /* -1 == last */
        }
      if (ok != NULL)
        {
          GtkCssProvider *prov = gtk_css_provider_new ();
          GtkStyleContext *ctx = gtk_widget_get_style_context (ok);

          /* The live preview replaces the theme accent: the button now
           * carries the chosen colour itself, which says more than a
           * generic "this is the primary action" tint would. */
          gtk_style_context_add_class (ctx, "lc-preview");
          gtk_style_context_add_provider (ctx, GTK_STYLE_PROVIDER (prov),
                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
          g_object_set_data_full (G_OBJECT (ok), "lc-preview-prov", prov, g_object_unref);

          g_signal_connect (dlg, "notify::rgba", G_CALLBACK (update_ok_preview), ok);
          update_ok_preview (G_OBJECT (dlg), NULL, ok);

          gtk_widget_set_can_default (ok, TRUE);
          gtk_widget_grab_default (ok);
        }
    }
  }
  {
    const gchar *cur = g_hash_table_lookup (colors, wc);
    GdkRGBA seed;
    if (cur != NULL && gdk_rgba_parse (&seed, cur))
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dlg), &seed);
  }
  {
    gint resp;

    /* 42 = pick from screen: handle it and keep the dialog open so the
     * user can still adjust or cancel. */
    while ((resp = gtk_dialog_run (GTK_DIALOG (dlg))) == 42)
      {
        GdkRGBA picked;
        if (pick_from_screen (dlg, &picked))
          gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dlg), &picked);
      }

    if (resp == GTK_RESPONSE_REJECT)
      {
        g_hash_table_remove (colors, wc);
        g_message ("FULL cleared colour for window %s (from dialog)", wc);
        rebuild_css ();
        retag_all ();
      }
    else if (resp == GTK_RESPONSE_OK)
    {
      GdkRGBA c;
      gchar a[G_ASCII_DTOSTR_BUF_SIZE];
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dlg), &c);
      g_ascii_formatd (a, sizeof a, "%.2f", c.alpha);
      g_hash_table_insert (colors, g_strdup (wc),
        g_strdup_printf ("rgba(%d,%d,%d,%s)",
          (int)(c.red*255+0.5), (int)(c.green*255+0.5), (int)(c.blue*255+0.5), a));
      rebuild_css ();
      retag_all ();
    }
  }
  gtk_widget_destroy (dlg);
  g_free (wc);
}

static gboolean
hook (GSignalInvocationHint *i, guint n, const GValue *pv, gpointer d)
{
  GtkWidget *w = GTK_WIDGET (g_value_get_object (&pv[0]));
  GtkWidget *att;
  (void) i; (void) n; (void) d;

  if (!GTK_IS_MENU (w))
    return TRUE;
  att = gtk_menu_get_attach_widget (GTK_MENU (w));
  /* ONLY the menu of a tasklist window button. */
  if (att == NULL || g_strcmp0 (G_OBJECT_TYPE_NAME (att), "XfceArrowButton") != 0)
    return TRUE;
  if (g_object_get_data (G_OBJECT (w), "lc-done") != NULL)
    return TRUE;
  g_object_set_data (G_OBJECT (w), "lc-done", GINT_TO_POINTER (1));

  {
    GtkWidget *sep = gtk_separator_menu_item_new ();
    GtkWidget *it = gtk_menu_item_new_with_label ("Color de fondo\xe2\x80\xa6");
    g_signal_connect (it, "activate", G_CALLBACK (on_choose), att);
    gtk_menu_shell_append (GTK_MENU_SHELL (w), sep);
    gtk_menu_shell_append (GTK_MENU_SHELL (w), it);
    gtk_widget_show (sep);
    gtk_widget_show (it);
    /* The menu was already sized before we appended, so GTK would add
     * scroll arrows instead of growing. Force a fresh size negotiation
     * and reposition; measured natural height is 400px against a 1394px
     * workarea, so there is ample room once GTK recomputes. */
    g_message ("FULL menu item inserted at show");
  }
  return TRUE;
}

G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);
G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
  (void) argc; (void) argv;
  if (g_strcmp0 (g_get_prgname (), "xfce4-panel") != 0)
    return;
  colors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
    GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  /* MEASURED: "show" fires while the menu is still unrealized and
   * unallocated (alloc_h=1), so an item appended there is present for the
   * first size negotiation and the menu gets its full natural height
   * (395px allocated == 395px natural, no scroll arrows). Appending at
   * "map" or even "realize" is too late: the menu is already sized to
   * 356px and GTK adds scroll arrows rather than growing it.
   * The menu is also rebuilt for every popup, so the per-object guard
   * must not assume it survives. */
  g_signal_add_emission_hook (g_signal_lookup ("show", GTK_TYPE_WIDGET), 0, hook, NULL, NULL);
  g_message ("FULL spike ready (show)");
}
