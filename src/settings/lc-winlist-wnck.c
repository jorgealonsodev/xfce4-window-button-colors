/* lc-winlist-wnck.c — see lc-winlist-wnck.h */
#include "lc-winlist-wnck.h"

#include <gdk/gdkx.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

/* Own WnckHandle, independent of src/glue/lc-tasklist.c's — that one is
 * compiled into the panel module (a different process/target entirely);
 * this settings binary needs its own libwnck client handle. Same
 * WNCK_CLIENT_TYPE_PAGER convention lc-tasklist.c uses, for the same
 * reason: this process, like the panel, only ever reads window state and
 * never acts as a window/task manager itself. */
static WnckScreen *
lc_winlist_wnck_get_screen (void)
{
  static WnckHandle *handle = NULL;

  if (handle == NULL)
    handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);

  return wnck_handle_get_default_screen (handle);
}

LcWinlistWnckSnapshot
lc_winlist_wnck_capture (GtkWidget *self_window)
{
  LcWinlistWnckSnapshot result = { FALSE, NULL, 0 };
  WnckScreen *screen;
  GdkWindow *gdk_window;
  gulong self_xid;
  GList *wnck_windows, *l;
  gsize n_live;
  LcWinlistWindow *windows;
  gsize i;
  gboolean self_xid_found = FALSE;

  g_return_val_if_fail (GTK_IS_WIDGET (self_window), result);

  /* Our own toplevel's GdkWindow must already exist — the map-gate
   * contract this file's header documents. A caller that violates it
   * gets an unavailable snapshot without a single libwnck call, rather
   * than a probe that is guaranteed to fail anyway. */
  gdk_window = gtk_widget_get_window (self_window);
  g_return_val_if_fail (gdk_window != NULL, result);

  self_xid = (gulong) GDK_WINDOW_XID (gdk_window);

  screen = lc_winlist_wnck_get_screen ();
  if (screen == NULL)
    return result; /* snapshot_ok stays FALSE: no WnckScreen at all */

  /* D3: the one and only wnck_screen_force_update() call site in this
   * project's settings process — see this file's header comment for
   * exactly why the module.c/lc-tasklist.c ban does not apply here. */
  wnck_screen_force_update (screen);

  wnck_windows = wnck_screen_get_windows (screen);
  n_live = (gsize) g_list_length (wnck_windows);

  if (n_live == 0)
    return result; /* obtained-but-empty (D2): snapshot_ok stays FALSE */

  windows = g_new0 (LcWinlistWindow, n_live);
  i = 0;
  for (l = wnck_windows; l != NULL; l = l->next)
    {
      WnckWindow *win = WNCK_WINDOW (l->data);
      gulong xid = (gulong) wnck_window_get_xid (win);

      windows[i].xid = xid;
      windows[i].title = wnck_window_get_name (win); /* borrowed, see header */
      i++;

      if (xid == self_xid)
        self_xid_found = TRUE;
    }

  if (!self_xid_found)
    {
      /* D2's completeness probe failed: a non-empty snapshot that still
       * does not include our own mapped window is proof it is partial,
       * not merely a suggestion. Discard it entirely rather than hand
       * back partial data — snapshot_ok stays FALSE. */
      g_free (windows);
      return result;
    }

  result.snapshot_ok = TRUE;
  result.windows = windows;
  result.n_windows = n_live;

  return result;
}

gulong *
lc_winlist_wnck_snapshot_xids (const LcWinlistWnckSnapshot *snapshot, gsize *out_n)
{
  gulong *xids;
  gsize i;

  g_return_val_if_fail (out_n != NULL, NULL);

  *out_n = 0;

  if (snapshot == NULL || !snapshot->snapshot_ok || snapshot->n_windows == 0)
    return NULL;

  xids = g_new (gulong, snapshot->n_windows);
  for (i = 0; i < snapshot->n_windows; i++)
    xids[i] = snapshot->windows[i].xid;

  *out_n = snapshot->n_windows;

  return xids;
}

void
lc_winlist_wnck_snapshot_clear (LcWinlistWnckSnapshot *snapshot)
{
  if (snapshot == NULL)
    return;

  g_free (snapshot->windows);
  snapshot->windows = NULL;
  snapshot->n_windows = 0;
  snapshot->snapshot_ok = FALSE;
}
