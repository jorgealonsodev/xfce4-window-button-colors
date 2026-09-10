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
  if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
      GdkRGBA c;
      gchar a[G_ASCII_DTOSTR_BUF_SIZE];
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dlg), &c);
      g_ascii_formatd (a, sizeof a, "%.2f", c.alpha);
      g_hash_table_insert (colors, g_strdup (wc),
        g_strdup_printf ("rgba(%d,%d,%d,%s)",
          (int)(c.red*255+0.5), (int)(c.green*255+0.5), (int)(c.blue*255+0.5), a));
      rebuild_css ();
      {
        GList *tops = gtk_window_list_toplevels (), *l;
        for (l = tops; l; l = l->next) retag (GTK_WIDGET (l->data));
        g_list_free (tops);
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
    g_message ("FULL menu item inserted on a tasklist button menu");
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
  g_signal_add_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), 0, hook, NULL, NULL);
  g_message ("FULL spike ready");
}
