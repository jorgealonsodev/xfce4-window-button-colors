/* main.c — entry point for the standalone settings application
 * (design.md "Technical Approach"): a separate GtkApplication process,
 * never loaded in-process by xfce4-panel, so it stays reachable exactly
 * in the state the user must escape (module disabled in /Gtk/Modules)
 * and its own failures can never abort the panel.
 *
 * Phase 4 built the bare shell; Phase 5 (design.md D1/D4/D5) added the
 * xfconf channel this process owns for its whole lifetime and embedded
 * lc-settings-ui.c's toggle row and restart action into the window.
 *
 * The stored-colour management section that Phase 7 added here has
 * since been removed (see lc-settings-ui.h) after it caused real,
 * unreproducible data loss; this process no longer needs an LcWinStore.
 *
 * Links gtk, gio and xfconf — this is src/settings/, where GTK and
 * xfconf are allowed to appear (design.md "Technical Approach");
 * src/core/ must never see either, which CI enforces by scanning
 * liblc-core.a for undefined gtk_/xfconf_/wnck_ symbols.
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

/* Bundles everything "activate" needs beyond the GtkApplication itself.
 * Owned by main() for the whole process lifetime (stack-allocated there,
 * alive for the full g_application_run() call) — lc-settings-ui.c only
 * ever borrows `channel` from this, exactly as its own header documents. */
typedef struct
{
  XfconfChannel *channel;
} LcSettingsAppContext;

static void
activate (GtkApplication *app, gpointer user_data)
{
  LcSettingsAppContext *ctx = user_data;
  GtkWidget *window;
  GtkWidget *ui;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Window Button Colors"));
  /* No fixed default size: the window asks GTK for whatever its content
   * needs and no more. A hardcoded 480x360 was sized for the stored-colour
   * list that used to sit below the restart action; with that section
   * withdrawn it left half the window empty. A width request keeps the
   * hint label from wrapping into a tall, narrow column, while the height
   * stays free to follow the content. */
  gtk_widget_set_size_request (window, 420, -1);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  /* lc-settings-ui.c: toggle row bound to lc_autoload_is_enabled()/
   * lc_autoload_set_enabled() via the xfconf backend, and the separate
   * restart action (D1/D4/D5). */
  ui = lc_settings_ui_new (ctx->channel);
  gtk_container_add (GTK_CONTAINER (window), ui);

  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  LcSettingsAppContext ctx = { NULL };
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

  ctx.channel = xfconf_channel_get (LC_AUTOLOAD_XFCONF_CHANNEL);

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
  g_signal_connect (app, "activate", G_CALLBACK (activate), &ctx);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  /* Deliberately NOT g_object_unref (ctx.channel) here: unlike
   * xfconf_channel_new(), xfconf_channel_get() (used above) is
   * G_GNUC_WARN_UNUSED_RESULT-free in xfconf-channel.h — it hands back
   * a reference into xfconf's own internal channel cache, which
   * xfconf_shutdown() below already walks and releases itself. Manually
   * unreffing it first (Phase 5's original shape here) drops the
   * channel's last reference early, and xfconf_shutdown()'s own cache
   * walk then unrefs the now-freed object — confirmed as a real
   * SIGSEGV in g_object_unref() from inside xfconf_shutdown(), caught
   * during Phase 7's mandated Xvfb clean-exit verification (task 7.3),
   * not a hypothetical. */
  xfconf_shutdown ();

  return status;
}
