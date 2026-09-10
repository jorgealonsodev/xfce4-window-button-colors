/* main.c — entry point for the standalone settings application
 * (design.md "Technical Approach"): a separate GtkApplication process,
 * never loaded in-process by xfce4-panel, so it stays reachable exactly
 * in the state the user must escape (module disabled in /Gtk/Modules)
 * and its own failures can never abort the panel.
 *
 * Phase 4 built the bare shell; Phase 5 (design.md D1/D4/D5) adds the
 * xfconf channel this process owns for its whole lifetime and embeds
 * lc-settings-ui.c's toggle row and restart action into the window.
 * The colour list is Phase 7.
 *
 * Links gtk, gio, xfconf and glib — this is src/settings/, where GTK,
 * xfconf and libwnck are allowed to appear (design.md "Technical
 * Approach"; src/core/ must never see any of them, which CI enforces
 * by scanning liblc-core.a for undefined gtk_/xfconf_/wnck_ symbols).
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "lc-autoload-xfconf.h"
#include "lc-settings-ui.h"

/* Reverse-DNS GApplication id. Does not need to match any D-Bus service
 * this project ships today — GtkApplication requires a well-formed id
 * even for a single-instance-by-default local application. */
#define LC_SETTINGS_APPLICATION_ID "io.github.jorgealonsodev.xfce4-window-button-colors-settings"

static void
activate (GtkApplication *app, gpointer user_data)
{
  XfconfChannel *channel = user_data;
  GtkWidget *window;
  GtkWidget *ui;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Window Button Colors"));
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 360);

  /* lc-settings-ui.c: toggle row bound to lc_autoload_is_enabled()/
   * lc_autoload_set_enabled() via the xfconf backend, plus the
   * separate restart action (D1/D4/D5). Phase 7 adds the colour list
   * below this same box. */
  ui = lc_settings_ui_new (channel);
  gtk_container_add (GTK_CONTAINER (window), ui);

  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  XfconfChannel *channel;
  GError *error = NULL;
  int status;

  /* Gettext domain binding, the same three-call sequence
   * src/glue/module.c uses for the panel module's own translations
   * (GETTEXT_PACKAGE/LOCALEDIR come from this target's c_args in
   * meson.build, mirroring lc_glue_c_args). */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* xfconf_init()/xfconf_channel_get() only ever READ /Gtk/Modules
   * until a user actually flips the toggle in lc-settings-ui.c — this
   * call alone never writes anything. One channel, obtained once, lives
   * for this process's whole run and is handed to lc-settings-ui.c. */
  if (!xfconf_init (&error))
    {
      g_printerr ("%s: %s\n", g_get_prgname (), error->message);
      g_error_free (error);
      return 1;
    }

  channel = xfconf_channel_get (LC_AUTOLOAD_XFCONF_CHANNEL);

  /* G_APPLICATION_DEFAULT_FLAGS replaces G_APPLICATION_FLAGS_NONE (same
   * value) starting glib 2.74; glib_dep's declared floor in meson.build
   * is 2.56, where the new name does not exist yet, so pick whichever
   * this build's headers actually provide instead of hardcoding one and
   * either warning under -Werror (old name, newer glib) or failing to
   * compile (new name, older glib). */
#if GLIB_CHECK_VERSION (2, 74, 0)
  app = gtk_application_new (LC_SETTINGS_APPLICATION_ID, G_APPLICATION_DEFAULT_FLAGS);
#else
  app = gtk_application_new (LC_SETTINGS_APPLICATION_ID, G_APPLICATION_FLAGS_NONE);
#endif
  g_signal_connect (app, "activate", G_CALLBACK (activate), channel);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);
  g_object_unref (channel);
  xfconf_shutdown ();

  return status;
}
