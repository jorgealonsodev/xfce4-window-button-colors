/* lc-tasklist.c — see lc-tasklist.h */
#include "lc-tasklist.h"

#include <string.h>
#include "lc-color.h"
#include "lc-provider.h"
#include "lc-winid.h"

struct _LcTasklistContext
{
  LcWinStore *store;   /* owned */
  gchar      *path;    /* owned */
  GdkScreen  *screen;  /* borrowed */
};

LcTasklistContext *
lc_tasklist_context_new (LcWinStore *store, const gchar *path, GdkScreen *screen)
{
  LcTasklistContext *ctx;

  g_return_val_if_fail (store != NULL, NULL);

  ctx = g_new0 (LcTasklistContext, 1);
  ctx->store = store;
  ctx->path = g_strdup (path);
  ctx->screen = screen;

  return ctx;
}

void
lc_tasklist_context_free (LcTasklistContext *ctx)
{
  if (ctx == NULL)
    return;

  lc_winstore_free (ctx->store);
  g_free (ctx->path);
  g_free (ctx);
}

LcWinStore *
lc_tasklist_context_store (LcTasklistContext *ctx)
{
  g_return_val_if_fail (ctx != NULL, NULL);

  return ctx->store;
}

void
lc_tasklist_context_set_screen (LcTasklistContext *ctx, GdkScreen *screen)
{
  g_return_if_fail (ctx != NULL);

  ctx->screen = screen;
}

WnckScreen *
lc_tasklist_get_wnck_screen (void)
{
  static WnckHandle *handle = NULL;

  if (handle == NULL)
    handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);

  return wnck_handle_get_default_screen (handle);
}

gboolean
lc_tasklist_is_window_button (GtkWidget *widget)
{
  GtkWidget *parent;
  const gchar *name;

  if (widget == NULL || !GTK_IS_WIDGET (widget))
    return FALSE;

  if (g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "XfceArrowButton") != 0)
    return FALSE;

  parent = gtk_widget_get_parent (widget);
  if (parent == NULL || g_strcmp0 (G_OBJECT_TYPE_NAME (parent), "XfceTasklist") != 0)
    return FALSE;

  name = gtk_widget_get_name (widget);
  if (g_strcmp0 (name, "panel-tasklist-arrow") == 0)
    return FALSE;

  return TRUE;
}

WnckWindow *
lc_tasklist_resolve_window (GtkWidget *button)
{
  gchar *tip;
  WnckScreen *screen;
  GtkWidget *parent;
  GPtrArray *same_title_buttons;
  GPtrArray *same_title_windows;
  GList *siblings, *l;
  guint rank = 0;
  gboolean found_rank = FALSE;
  guint i;
  WnckWindow *result = NULL;

  if (!lc_tasklist_is_window_button (button))
    return NULL;

  tip = gtk_widget_get_tooltip_text (button);
  if (tip == NULL || tip[0] == '\0')
    {
      g_free (tip);
      return NULL;
    }

  screen = lc_tasklist_get_wnck_screen ();
  if (screen == NULL)
    {
      g_free (tip);
      return NULL;
    }

  /* Deliberately no wnck_screen_force_update() here: this function only
   * ever runs from the main loop (the "map"/"show" hooks, or a
   * user-triggered menu/dialog callback), long after wnck has an
   * up-to-date window list maintained by its own event handling.
   * force_update() synchronously re-enters libwnck's client-list update,
   * and the panel's own tasklist plugin is itself a libwnck client — on
   * a live panel this produced a reentrancy assertion (harmless-looking,
   * but genuinely risks handing back a half-updated list, and aborts
   * outright under G_DEBUG=fatal-criticals). See module.c's
   * lc_collect_live_xids() for the other call site this was also
   * removed from. */

  /* Rank `button` among its tasklist siblings sharing this exact tooltip
   * text, in stable container child order. */
  parent = gtk_widget_get_parent (button);
  same_title_buttons = g_ptr_array_new ();
  siblings = gtk_container_get_children (GTK_CONTAINER (parent));
  for (l = siblings; l != NULL; l = l->next)
    {
      GtkWidget *sibling = GTK_WIDGET (l->data);
      gchar *sibling_tip;

      if (!lc_tasklist_is_window_button (sibling))
        continue;

      sibling_tip = gtk_widget_get_tooltip_text (sibling);
      if (g_strcmp0 (sibling_tip, tip) == 0)
        g_ptr_array_add (same_title_buttons, sibling);
      g_free (sibling_tip);
    }
  g_list_free (siblings);

  for (i = 0; i < same_title_buttons->len; i++)
    {
      if (g_ptr_array_index (same_title_buttons, i) == button)
        {
          rank = i;
          found_rank = TRUE;
          break;
        }
    }
  g_ptr_array_free (same_title_buttons, TRUE);

  if (!found_rank)
    {
      g_free (tip);
      return NULL;
    }

  /* Pair that rank with the same rank among the live windows sharing the
   * identical title, in wnck_screen_get_windows() order. */
  same_title_windows = g_ptr_array_new ();
  for (l = wnck_screen_get_windows (screen); l != NULL; l = l->next)
    {
      WnckWindow *win = WNCK_WINDOW (l->data);

      if (g_strcmp0 (wnck_window_get_name (win), tip) == 0)
        g_ptr_array_add (same_title_windows, win);
    }

  if (rank < same_title_windows->len)
    result = g_ptr_array_index (same_title_windows, rank);

  g_ptr_array_free (same_title_windows, TRUE);
  g_free (tip);

  return result;
}

void
lc_tasklist_retag_button (GtkWidget *button, LcWinStore *store)
{
  GtkStyleContext *ctx;
  GList *classes, *c;
  WnckWindow *win;

  if (!lc_tasklist_is_window_button (button))
    return;

  ctx = gtk_widget_get_style_context (button);

  /* Strip any existing lc-win-* class first: a button GTK/XFCE recycles
   * for a different window must never keep painting the old one. */
  classes = gtk_style_context_list_classes (ctx);
  for (c = classes; c != NULL; c = c->next)
    {
      const gchar *cls = c->data;

      if (g_str_has_prefix (cls, "lc-win-"))
        gtk_style_context_remove_class (ctx, cls);
    }
  g_list_free (classes);

  win = lc_tasklist_resolve_window (button);
  if (win == NULL)
    return;

  {
    gulong xid = (gulong) wnck_window_get_xid (win);

    if (lc_winstore_has (store, xid))
      {
        gchar *cls = lc_winid_css_class (xid);

        if (cls != NULL)
          {
            gtk_style_context_add_class (ctx, cls);
            g_free (cls);
          }
      }
  }
}

static gboolean
lc_tasklist_map_hook (GSignalInvocationHint *hint, guint n_params, const GValue *params,
                       gpointer user_data)
{
  GtkWidget *widget = GTK_WIDGET (g_value_get_object (&params[0]));
  LcTasklistContext *ctx = user_data;

  (void) hint;
  (void) n_params;

  if (lc_tasklist_is_window_button (widget))
    lc_tasklist_retag_button (widget, ctx->store);

  return TRUE; /* keep the hook installed for the next "map" */
}

gulong
lc_tasklist_install_hook (LcTasklistContext *ctx)
{
  g_return_val_if_fail (ctx != NULL, 0);

  return g_signal_add_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), 0,
                                      lc_tasklist_map_hook, ctx, NULL);
}

void
lc_tasklist_uninstall_hook (gulong hook_id)
{
  if (hook_id == 0)
    return;

  g_signal_remove_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), hook_id);
}

void
lc_tasklist_apply_change (LcTasklistContext *ctx, GtkWidget *button)
{
  LcDocument *doc;

  g_return_if_fail (ctx != NULL);

  doc = lc_winstore_render (ctx->store);
  lc_provider_attach (ctx->screen, doc);
  lc_document_free (doc);

  if (ctx->path != NULL && !lc_winstore_save (ctx->store, ctx->path))
    g_warning ("xfce4-window-button-colors: could not save window colours to '%s'", ctx->path);

  if (button != NULL)
    lc_tasklist_retag_button (button, ctx->store);
}

void
lc_tasklist_render_and_attach (LcTasklistContext *ctx)
{
  LcDocument *doc;

  g_return_if_fail (ctx != NULL);

  /* Deliberately no lc_winstore_save() here: nothing was reconciled or
   * otherwise mutated, so there is nothing to persist. */
  doc = lc_winstore_render (ctx->store);
  lc_provider_attach (ctx->screen, doc);
  lc_document_free (doc);
}

static void
lc_tasklist_on_window_closed (WnckScreen *screen, WnckWindow *window, gpointer user_data)
{
  LcTasklistContext *ctx = user_data;
  gulong xid;

  (void) screen;

  if (window == NULL)
    return;

  xid = (gulong) wnck_window_get_xid (window);

  /* Positive evidence only: this exact window has just closed. Unset it
   * if (and only if) store actually held a colour for it — lc_winstore_unset()
   * returning TRUE means a real change happened, so persisting via
   * lc_tasklist_apply_change() is correct here (unlike the startup path,
   * where nothing changed and saving would be wrong). */
  if (lc_winstore_unset (ctx->store, xid))
    lc_tasklist_apply_change (ctx, NULL);
}

gulong
lc_tasklist_install_window_closed_hook (LcTasklistContext *ctx)
{
  WnckScreen *screen;

  g_return_val_if_fail (ctx != NULL, 0);

  screen = lc_tasklist_get_wnck_screen ();
  if (screen == NULL)
    return 0;

  return g_signal_connect (screen, "window-closed", G_CALLBACK (lc_tasklist_on_window_closed), ctx);
}

void
lc_tasklist_uninstall_window_closed_hook (gulong handler_id)
{
  WnckScreen *screen;

  if (handler_id == 0)
    return;

  screen = lc_tasklist_get_wnck_screen ();
  if (screen != NULL)
    g_signal_handler_disconnect (screen, handler_id);
}
