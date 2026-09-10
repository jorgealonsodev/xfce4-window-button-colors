/* lc-dialogs.c — see lc-dialogs.h */
#include "lc-dialogs.h"

#include <glib/gi18n.h>
#include "lc-color.h"

/* Custom GtkDialog response id for the eyedropper action button. Chosen
 * well above the reserved GTK_RESPONSE_* negative range and above any
 * positive id GTK itself assigns; the exact value only has to be
 * distinct from GTK_RESPONSE_OK/CANCEL/REJECT/DELETE_EVENT. */
#define LC_RESPONSE_PICK 100

/* Kept alive for the whole time the dialog is open. `button` is tracked
 * with a weak pointer so if the window/button goes away while the async
 * dialog is still open (its own window closed, the tasklist rebuilding),
 * lc_tasklist_apply_change() below is simply told there is no button to
 * retag rather than touching a dangling pointer. */
typedef struct
{
  LcTasklistContext *ctx;
  GtkWidget         *button; /* weak pointer, may become NULL */
  gulong             xid;
} LcDialogClosure;

static void
lc_dialog_closure_free (LcDialogClosure *closure)
{
  if (closure == NULL)
    return;

  if (closure->button != NULL)
    g_object_remove_weak_pointer (G_OBJECT (closure->button), (gpointer *) &closure->button);

  g_free (closure);
}

/* Live preview: paint the confirming (OK) button with the colour being
 * chosen, switching its label colour black/white by relative luminance
 * so it stays readable against every hue. The alpha channel MUST go
 * through g_ascii_formatd(), never printf("%f")/"%.2f" — see the project
 * header rule this mirrors, and lc_color.c's identical safeguard. */
static void
lc_dialogs_update_preview (GtkColorChooser *chooser, GParamSpec *pspec, gpointer user_data)
{
  GtkWidget *ok = user_data;
  GtkCssProvider *provider = g_object_get_data (G_OBJECT (ok), "lc-preview-provider");
  GdkRGBA rgba;
  gchar alpha_text[G_ASCII_DTOSTR_BUF_SIZE];
  gdouble luminance;
  gchar *css;

  (void) pspec;

  if (provider == NULL)
    return;

  gtk_color_chooser_get_rgba (chooser, &rgba);

  g_ascii_formatd (alpha_text, sizeof alpha_text, "%.2f", rgba.alpha);

  luminance = 0.2126 * rgba.red + 0.7152 * rgba.green + 0.0722 * rgba.blue;

  css = g_strdup_printf (
    "button.lc-preview {"
    "  background-image: none;"
    "  background-color: rgba(%d,%d,%d,%s);"
    "  color: %s;"
    "  text-shadow: none;"
    "}",
    (int) (rgba.red * 255 + 0.5), (int) (rgba.green * 255 + 0.5), (int) (rgba.blue * 255 + 0.5),
    alpha_text,
    luminance > 0.55 ? "#000000" : "#ffffff");

  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  g_free (css);
}

static void
lc_dialogs_wire_preview (GtkDialog *dialog)
{
  GtkWidget *ok = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
  GtkCssProvider *provider;
  GtkStyleContext *style_ctx;

  if (ok == NULL)
    return;

  provider = gtk_css_provider_new ();
  style_ctx = gtk_widget_get_style_context (ok);

  gtk_style_context_add_class (style_ctx, "lc-preview");
  gtk_style_context_add_provider (style_ctx, GTK_STYLE_PROVIDER (provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_set_data_full (G_OBJECT (ok), "lc-preview-provider", provider, g_object_unref);

  g_signal_connect (dialog, "notify::rgba", G_CALLBACK (lc_dialogs_update_preview), ok);
  lc_dialogs_update_preview (GTK_COLOR_CHOOSER (dialog), NULL, ok);

  gtk_widget_set_can_default (ok, TRUE);
  gtk_widget_grab_default (ok);
}

/* Adds the eyedropper as an icon button, with a themed-icon fallback
 * chain and a tooltip so it is never a bare unlabelled glyph. */
static GtkWidget *
lc_dialogs_add_eyedropper_button (GtkDialog *dialog)
{
  static const gchar *icon_names[] = {
    "color-select-symbolic", "gtk-color-picker", "color-picker", "color-select", NULL
  };
  GtkIconTheme *theme = gtk_icon_theme_get_default ();
  const gchar *chosen = NULL;
  GtkWidget *button;
  gsize i;

  for (i = 0; icon_names[i] != NULL && chosen == NULL; i++)
    if (gtk_icon_theme_has_icon (theme, icon_names[i]))
      chosen = icon_names[i];

  if (chosen != NULL)
    {
      button = gtk_button_new ();
      gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (chosen, GTK_ICON_SIZE_BUTTON));
      gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
    }
  else
    button = gtk_button_new_with_label (_("Eyedropper"));

  /* The icon alone is not self-explanatory to everyone, so the name
   * stays reachable as a tooltip regardless of which fallback was used. */
  gtk_widget_set_tooltip_text (button, _("Pick a colour from the screen"));

  gtk_dialog_add_action_widget (dialog, button, LC_RESPONSE_PICK);
  gtk_widget_show_all (button);

  return button;
}

/* Button order and emphasis: the confirming action (OK) must be last and
 * read as the primary one; "No colour" and the eyedropper are auxiliary
 * and belong in the secondary button-box group, away from the commit
 * button. */
static void
lc_dialogs_reorder_actions (GtkDialog *dialog, GtkWidget *eyedropper)
{
  GtkWidget *ok = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
  GtkWidget *cancel = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_CANCEL);
  GtkWidget *no_colour = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_REJECT);
  GtkWidget *area = ok != NULL ? gtk_widget_get_parent (ok) : NULL;

  if (area == NULL)
    return;

  if (GTK_IS_BUTTON_BOX (area))
    {
      if (no_colour != NULL)
        gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (area), no_colour, TRUE);
      if (eyedropper != NULL)
        gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (area), eyedropper, TRUE);
    }

  if (GTK_IS_BOX (area))
    {
      if (cancel != NULL)
        gtk_box_reorder_child (GTK_BOX (area), cancel, -1);
      if (ok != NULL)
        gtk_box_reorder_child (GTK_BOX (area), ok, -1); /* -1 == last */
    }
}

/* ---- async screen-colour pick ---------------------------------------
 *
 * Event-driven replacement for the spike's `while (!p.done)
 * gtk_main_iteration();` nested wait: a pointer grab is started here and
 * this function returns immediately. The actual pick happens later, from
 * the ordinary GTK main loop, inside lc_dialogs_on_pick_press() below.
 */
typedef struct
{
  GtkWidget *dialog;
  gulong     handler_id;
  GdkCursor *cursor;
} LcPickState;

static gboolean
lc_dialogs_on_pick_press (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  LcPickState *pick = user_data;
  GdkSeat *seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));

  gdk_seat_ungrab (seat);
  g_signal_handler_disconnect (widget, pick->handler_id);
  if (pick->cursor != NULL)
    g_object_unref (pick->cursor);

  if (event->button == 1) /* left button picks; any other cancels */
    {
      GdkWindow *root = gdk_get_default_root_window ();
      GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window (root, (gint) event->x_root, (gint) event->y_root, 1, 1);

      if (pixbuf != NULL)
        {
          guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
          GdkRGBA picked;

          picked.red = pixels[0] / 255.0;
          picked.green = pixels[1] / 255.0;
          picked.blue = pixels[2] / 255.0;
          picked.alpha = 0.85; /* keep the tag translucent by default */

          gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (pick->dialog), &picked);
          g_object_unref (pixbuf);
        }
      else
        {
          g_warning ("xfce4-window-button-colors: could not read the pixel under the pointer");
        }
    }

  g_free (pick);

  return TRUE;
}

static void
lc_dialogs_begin_pick (GtkWidget *dialog)
{
  GdkDisplay *display = gtk_widget_get_display (dialog);
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkWindow *gdk_win = gtk_widget_get_window (dialog);
  LcPickState *pick;

  if (gdk_win == NULL)
    return;

  pick = g_new0 (LcPickState, 1);
  pick->dialog = dialog;
  pick->cursor = gdk_cursor_new_from_name (display, "crosshair");

  gtk_widget_add_events (dialog, GDK_BUTTON_PRESS_MASK);
  pick->handler_id = g_signal_connect (dialog, "button-press-event",
                                        G_CALLBACK (lc_dialogs_on_pick_press), pick);

  if (gdk_seat_grab (seat, gdk_win, GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE,
                      pick->cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
    {
      g_warning ("xfce4-window-button-colors: could not grab the pointer to pick a colour");
      g_signal_handler_disconnect (dialog, pick->handler_id);
      if (pick->cursor != NULL)
        g_object_unref (pick->cursor);
      g_free (pick);
    }
}

static void
lc_dialogs_on_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  LcDialogClosure *closure = user_data;

  if (response_id == LC_RESPONSE_PICK)
    {
      /* Handled asynchronously; the dialog stays open either way. */
      lc_dialogs_begin_pick (GTK_WIDGET (dialog));
      return;
    }

  if (response_id == GTK_RESPONSE_OK)
    {
      GdkRGBA rgba;
      LcColor color;

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);
      color.r = (guint8) CLAMP ((gint) (rgba.red * 255.0 + 0.5), 0, 255);
      color.g = (guint8) CLAMP ((gint) (rgba.green * 255.0 + 0.5), 0, 255);
      color.b = (guint8) CLAMP ((gint) (rgba.blue * 255.0 + 0.5), 0, 255);
      color.a = (guint8) CLAMP ((gint) (rgba.alpha * 255.0 + 0.5), 0, 255);

      lc_winstore_set (lc_tasklist_context_store (closure->ctx), closure->xid, &color);
      lc_tasklist_apply_change (closure->ctx, closure->button);
    }
  else if (response_id == GTK_RESPONSE_REJECT) /* "No colour" */
    {
      lc_winstore_unset (lc_tasklist_context_store (closure->ctx), closure->xid);
      lc_tasklist_apply_change (closure->ctx, closure->button);
    }
  /* GTK_RESPONSE_CANCEL / GTK_RESPONSE_DELETE_EVENT / anything else:
   * close with no change. */

  gtk_widget_destroy (GTK_WIDGET (dialog));
  lc_dialog_closure_free (closure);
}

void
lc_dialogs_open_for_button (LcTasklistContext *ctx, GtkWidget *button)
{
  WnckWindow *win;
  gulong xid;
  GtkWidget *toplevel;
  GtkWidget *dialog;
  GtkWidget *eyedropper;
  LcColor current;
  LcDialogClosure *closure;

  g_return_if_fail (ctx != NULL);
  g_return_if_fail (button != NULL);

  win = lc_tasklist_resolve_window (button);
  if (win == NULL)
    {
      g_warning ("xfce4-window-button-colors: could not resolve the window for this button");
      return;
    }
  xid = (gulong) wnck_window_get_xid (win);

  toplevel = gtk_widget_get_toplevel (button);
  dialog = gtk_color_chooser_dialog_new (_("Background colour"),
                                          GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL);
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (dialog), TRUE);

  /* Do NOT call gtk_color_chooser_add_palette() here: its first call
   * removes GTK's own default palette (documented behaviour), and the
   * submenu already provides the quick palette. This dialog's job is the
   * full range, deliberately not unified with the submenu's eight. */

  if (lc_winstore_get (lc_tasklist_context_store (ctx), xid, &current))
    {
      GdkRGBA seed;

      seed.red = current.r / 255.0;
      seed.green = current.g / 255.0;
      seed.blue = current.b / 255.0;
      seed.alpha = current.a / 255.0;
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &seed);
    }

  /* Removing the colour must be reachable from inside the dialog too:
   * the INVARIANT that every colour-selecting surface can also select
   * none applies here just as much as in the submenu. */
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("No colour"), GTK_RESPONSE_REJECT);

  eyedropper = lc_dialogs_add_eyedropper_button (GTK_DIALOG (dialog));
  lc_dialogs_reorder_actions (GTK_DIALOG (dialog), eyedropper);
  lc_dialogs_wire_preview (GTK_DIALOG (dialog));

  closure = g_new0 (LcDialogClosure, 1);
  closure->ctx = ctx;
  closure->button = button;
  closure->xid = xid;
  g_object_add_weak_pointer (G_OBJECT (button), (gpointer *) &closure->button);

  g_signal_connect (dialog, "response", G_CALLBACK (lc_dialogs_on_response), closure);

  gtk_widget_show (dialog);
}
