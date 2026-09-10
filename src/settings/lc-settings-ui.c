/* lc-settings-ui.c — see lc-settings-ui.h.
 *
 * No gtk_dialog_run() anywhere in this file, matching lc-dialogs.c's
 * established convention (its header explains why: a nested main loop
 * can act on already-destroyed widgets). This process is standalone
 * rather than in-process with xfce4-panel (design.md D3), but the same
 * async "response" signal pattern is followed for consistency and to
 * avoid a nested loop wedging this window while the user is mid-decision
 * on the restart confirmation.
 */
#include "lc-settings-ui.h"

#include "lc-autoload.h"
#include "lc-autoload-xfconf.h"
#include "lc-restart.h"

#include <glib/gi18n.h>

/* The sole real backend (lc-autoload-xfconf.h) — same instance for
 * every call lc_autoload_set_enabled()/refresh make, matching the
 * "static const" pattern its own header comment shows as the intended
 * call site. */
static const LcAutoloadBackend lc_settings_ui_backend = {
  lc_autoload_xfconf_read,
  lc_autoload_xfconf_write,
};

/* Kept alive for as long as the returned box lives; freed from the
 * box's own "destroy" handler, which also disconnects `channel`'s
 * property-changed handler (`channel` itself is owned by the caller,
 * not by this struct). */
typedef struct
{
  XfconfChannel *channel;
  GtkWidget     *toggle;      /* GtkSwitch */
  gulong         toggle_handler_id;
  GtkWidget     *hint_label;
  gulong         property_changed_handler_id;
} LcSettingsUi;

/* ---- shared error/hint surfacing --------------------------------------- */

/* One non-modal error dialog. `command`, when non-NULL, is shown below
 * the message in a selectable label so the user can copy it — required
 * for spawn failures (D4) so the literal failed command is recoverable
 * without retyping it from memory. No automatic retry anywhere in this
 * file. */
static void
lc_settings_ui_show_error (GtkWidget *related_widget, const gchar *message, const gchar *command)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (related_widget);
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                    "%s", message);
  gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);

  if (command != NULL)
    {
      GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
      GtkWidget *label = gtk_label_new (command);

      gtk_label_set_selectable (GTK_LABEL (label), TRUE);
      gtk_widget_set_margin_top (label, 6);
      gtk_widget_set_margin_start (label, 6);
      gtk_widget_set_margin_end (label, 6);
      gtk_box_pack_start (GTK_BOX (content), label, FALSE, FALSE, 0);
      gtk_widget_show (label);
    }

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_widget_show (dialog);
}

static void
lc_settings_ui_show_hint (LcSettingsUi *ui)
{
  gtk_widget_show (ui->hint_label);
}

/* ---- toggle: reflects xfconf state, never restarts --------------------- */

/* Re-reads the current enabled state from `ui->channel` and sets the
 * switch to match, with the switch's own toggle handler blocked so
 * this never triggers a write (D5 — "the handler is blocked while
 * reflecting, or you will loop"). Used both at startup and to reflect
 * an external xfconf change or revert a failed write. */
static void
lc_settings_ui_refresh (LcSettingsUi *ui)
{
  LcAutoloadShape shape = LC_AUTOLOAD_SHAPE_ABSENT;
  gchar **modules = NULL;
  gboolean enabled = FALSE;

  if (lc_autoload_xfconf_read (ui->channel, &shape, &modules)
      && (shape == LC_AUTOLOAD_SHAPE_SCALAR || shape == LC_AUTOLOAD_SHAPE_ARRAY))
    enabled = lc_autoload_is_enabled ((const gchar *const *) modules, LC_AUTOLOAD_MODULE_NAME);

  /* ABSENT, UNKNOWN, and a failed read all mean "not enabled" here;
   * UNKNOWN additionally means a toggle attempt will report
   * LC_AUTOLOAD_UNSUPPORTED_SHAPE rather than silently guessing. */
  g_strfreev (modules);

  g_signal_handler_block (ui->toggle, ui->toggle_handler_id);
  gtk_switch_set_active (GTK_SWITCH (ui->toggle), enabled);
  g_signal_handler_unblock (ui->toggle, ui->toggle_handler_id);
}

static void
lc_settings_ui_on_toggle_active_notify (GtkSwitch *widget, GParamSpec *pspec, gpointer user_data)
{
  LcSettingsUi *ui = user_data;
  gboolean enabled = gtk_switch_get_active (widget);
  LcAutoloadResult result;

  (void) pspec;

  /* The xfconf write completes here, in full, before this function
   * returns — the panel is never restarted as a side effect
   * (module-autoload-toggle "Toggle completes without restarting"). */
  result = lc_autoload_set_enabled (&lc_settings_ui_backend, ui->channel,
                                     LC_AUTOLOAD_MODULE_NAME, enabled);

  switch (result)
    {
    case LC_AUTOLOAD_OK:
      lc_settings_ui_show_hint (ui);
      break;

    case LC_AUTOLOAD_NO_CHANGE:
      /* Already in the requested state — nothing to do. */
      break;

    case LC_AUTOLOAD_READ_FAILED:
      lc_settings_ui_refresh (ui);
      lc_settings_ui_show_error (GTK_WIDGET (widget),
                                  _("Could not read the module autoload setting."), NULL);
      break;

    case LC_AUTOLOAD_UNSUPPORTED_SHAPE:
      lc_settings_ui_refresh (ui);
      lc_settings_ui_show_error (
        GTK_WIDGET (widget),
        _("The /Gtk/Modules setting has an unexpected type and was left untouched."), NULL);
      break;

    case LC_AUTOLOAD_WRITE_FAILED:
    default:
      lc_settings_ui_refresh (ui);
      lc_settings_ui_show_error (GTK_WIDGET (widget),
                                  _("Could not write the module autoload setting."), NULL);
      break;
    }
}

/* D5: xfconf is the only source GTK actually reads, so a concurrent
 * `xfconf-query` edit or a second instance of this app must converge
 * here too. Filtered to /Gtk/Modules; every other property on this
 * channel is ignored. */
static void
lc_settings_ui_on_property_changed (XfconfChannel *channel, const gchar *property,
                                     const GValue *value, gpointer user_data)
{
  LcSettingsUi *ui = user_data;

  (void) channel;
  (void) value;

  if (g_strcmp0 (property, LC_AUTOLOAD_XFCONF_KEY) != 0)
    return;

  lc_settings_ui_refresh (ui);
  lc_settings_ui_show_hint (ui);
}

/* ---- restart: separate, explicit, cancellable (D4) ---------------------- */

static void
lc_settings_ui_child_watch (GPid pid, gint status, gpointer user_data)
{
  (void) status;
  (void) user_data;

  /* G_SPAWN_DO_NOT_REAP_CHILD requires the caller to reap the child
   * itself; this is that reap. Nothing else needs the exit status. */
  g_spawn_close_pid (pid);
}

static void
lc_settings_ui_restart_spawn (GtkWidget *related_widget)
{
  gchar **argv = lc_restart_panel_argv ();
  GError *error = NULL;
  GPid pid = 0;

  /* Always the pure argv vector, always g_spawn_async() — never
   * g_spawn_command_line_*() and never a shell string (D4). */
  if (g_spawn_async (NULL, argv, NULL,
                      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                      NULL, NULL, &pid, &error))
    {
      g_child_watch_add (pid, lc_settings_ui_child_watch, NULL);
    }
  else
    {
      gchar *command = g_strjoinv (" ", argv);
      gchar *message = g_strdup_printf (_("Could not restart the panel: %s"), error->message);

      /* The xfconf write, if any, already committed independently and
       * is idempotent — a failed spawn leaves no partial state. */
      lc_settings_ui_show_error (related_widget, message, command);

      g_free (message);
      g_free (command);
      g_error_free (error);
    }

  g_strfreev (argv);
}

static void
lc_settings_ui_on_restart_confirm_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  GtkWidget *related_widget = user_data;

  if (response_id == GTK_RESPONSE_ACCEPT)
    lc_settings_ui_restart_spawn (related_widget);
  /* Any other response (Cancel, Escape, window close): no restart; the
   * xfconf change, if any, persists exactly as written
   * (module-autoload-toggle "Restart is cancellable"). */

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
lc_settings_ui_on_restart_clicked (GtkButton *button, gpointer user_data)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  GtkWidget *dialog;

  (void) user_data;

  dialog = gtk_message_dialog_new (
    GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL, "%s", _("Restart the panel now?"));

  /* Names the exact TTY recovery command from README.md:113, in the
   * same breath as the confirmation, so a user about to take the risky
   * action can read how to undo it before confirming. */
  gtk_message_dialog_format_secondary_text (
    GTK_MESSAGE_DIALOG (dialog), "%s",
    _("The panel will briefly disappear and come back. If it does not come back, recover "
      "from a TTY with: xfconf-query -c xsettings -p /Gtk/Modules -r, then log in again."));

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Restart"), GTK_RESPONSE_ACCEPT);

  g_signal_connect (dialog, "response", G_CALLBACK (lc_settings_ui_on_restart_confirm_response),
                     button);

  gtk_widget_show (dialog);
}

/* ---- lifecycle ------------------------------------------------------- */

static void
lc_settings_ui_on_box_destroy (GtkWidget *box, gpointer user_data)
{
  LcSettingsUi *ui = user_data;

  (void) box;

  /* `ui->channel` is disconnected explicitly because it is NOT a child
   * of `box` — it is borrowed from the caller (main.c) and outlives
   * this widget, so its "property-changed" handler must be removed by
   * hand or it would keep calling back into this about-to-be-freed
   * `ui` (D5). `ui->toggle`, by contrast, IS a child of `box`: GTK's
   * own container destruction already tears it down (and every signal
   * handler connected to it) as part of this same gtk_widget_destroy()
   * cascade. */
  g_signal_handler_disconnect (ui->channel, ui->property_changed_handler_id);
  g_free (ui);
}

GtkWidget *
lc_settings_ui_new (XfconfChannel *channel)
{
  LcSettingsUi *ui;
  GtkWidget *box;
  GtkWidget *toggle_row;
  GtkWidget *label;
  GtkWidget *restart_button;

  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

  ui = g_new0 (LcSettingsUi, 1);
  ui->channel = channel;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);

  toggle_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new (_("Enable window button colours"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_pack_start (GTK_BOX (toggle_row), label, TRUE, TRUE, 0);

  ui->toggle = gtk_switch_new ();
  gtk_widget_set_halign (ui->toggle, GTK_ALIGN_END);
  gtk_box_pack_start (GTK_BOX (toggle_row), ui->toggle, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box), toggle_row, FALSE, FALSE, 0);

  ui->hint_label = gtk_label_new (
    _("Configuration changed since this window opened — restart the panel to apply it."));
  gtk_widget_set_halign (ui->hint_label, GTK_ALIGN_START);
  gtk_label_set_line_wrap (GTK_LABEL (ui->hint_label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), ui->hint_label, FALSE, FALSE, 0);

  restart_button = gtk_button_new_with_mnemonic (_("_Restart Panel"));
  gtk_widget_set_halign (restart_button, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), restart_button, FALSE, FALSE, 0);

  ui->toggle_handler_id = g_signal_connect (ui->toggle, "notify::active",
                                             G_CALLBACK (lc_settings_ui_on_toggle_active_notify), ui);
  /* The restart action stays enabled regardless of the hint (D5) — it
   * is never gated on `ui->hint_label`'s visibility. */
  g_signal_connect (restart_button, "clicked", G_CALLBACK (lc_settings_ui_on_restart_clicked), NULL);

  ui->property_changed_handler_id = g_signal_connect (
    channel, "property-changed", G_CALLBACK (lc_settings_ui_on_property_changed), ui);

  g_signal_connect (box, "destroy", G_CALLBACK (lc_settings_ui_on_box_destroy), ui);

  gtk_widget_show_all (box);
  gtk_widget_hide (ui->hint_label);

  lc_settings_ui_refresh (ui);

  return box;
}
