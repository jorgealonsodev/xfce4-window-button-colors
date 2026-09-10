/* main.c — entry point for the standalone settings application
 * (design.md "Technical Approach"): a separate GtkApplication process,
 * never loaded in-process by xfce4-panel, so it stays reachable exactly
 * in the state the user must escape (module disabled in /Gtk/Modules)
 * and its own failures can never abort the panel.
 *
 * This is Phase 4's shell only: GtkApplication bootstrap, gettext
 * domain binding, and a bare window. The toggle row, the restart
 * action, and the colour list are Phase 5 onward (design.md D1/D4/D5,
 * tasks.md Phase 5+) — nothing here reads or writes xfconf yet, and
 * lc-autoload-xfconf.c's backend is not wired to any widget in this
 * slice.
 *
 * Links gtk, gio and glib — this is src/settings/, where GTK, xfconf
 * and libwnck are allowed to appear (design.md "Technical Approach";
 * src/core/ must never see any of them, which CI enforces by scanning
 * liblc-core.a for undefined gtk_/xfconf_/wnck_ symbols).
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* Reverse-DNS GApplication id. Does not need to match any D-Bus service
 * this project ships today — GtkApplication requires a well-formed id
 * even for a single-instance-by-default local application. */
#define LC_SETTINGS_APPLICATION_ID "io.github.jorgealonsodev.xfce4-window-button-colors-settings"

static void
activate (GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;

  (void) user_data;

  /* Phase 4 shell: a plain, empty window. Phase 5 replaces this body
   * with lc-settings-ui.c's toggle row and restart action; Phase 7
   * adds the colour list. */
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Window Button Colors"));
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 360);
  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  /* Gettext domain binding, the same three-call sequence
   * src/glue/module.c uses for the panel module's own translations
   * (GETTEXT_PACKAGE/LOCALEDIR come from this target's c_args in
   * meson.build, mirroring lc_glue_c_args). */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

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
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
