/* harness-app.c — throwaway, non-panel GTK3 process for OFFLINE
 * verification of the module lifecycle (tasks.md 3.6). NOT installed,
 * NOT linked against the module or lc-core, and never loaded into the
 * real xfce4-panel.
 *
 * This program is a stand-in for "the panel": it does nothing but build
 * one launcher-shaped widget hierarchy and print the inner widget's
 * resolved background colour. The module itself is loaded the same way
 * XFCE would load it — via the GTK_MODULES environment variable, scoped
 * to this one process — never via a written xfconf key, so the real
 * panel and the user's session-wide /Gtk/Modules key are never touched.
 *
 * Usage (see docs/HANDOFF.md's testing section for the live-panel
 * equivalent, which this intentionally does NOT run):
 *
 *   GTK_MODULES=/abs/path/to/build/libxfce4-launcher-colors.so \
 *   HOME=/some/temp/xdg/home \
 *   ./harness-app [prgname] [launcher-id]
 *
 * `prgname` (default "xfce4-panel") is applied via g_set_prgname() BEFORE
 * gtk_init(), because GTK_MODULES modules are loaded during gtk_init()
 * and lc_guard_should_activate() reads g_get_prgname() at that point.
 * Passing anything other than "xfce4-panel" reproduces the guard-inert
 * case without needing a second binary.
 *
 * `launcher-id` (default "5") names the outer widget "launcher-<id>", to
 * match a hand-written colors.css record for that id.
 *
 * Output: one line "bg=<r>,<g>,<b>,<a>" (0.0-1.0 floats), the resolved
 * background-color of the widget named "launcher-arrow" nested inside
 * "launcher-<id>" — the exact selector shape lc_id_selector_new()
 * produces (design.md D5). Exit code is always 0 unless GTK itself fails
 * to initialize (e.g. no display).
 */
#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
  const gchar *prgname = (argc > 1) ? argv[1] : "xfce4-panel";
  const gchar *id = (argc > 2) ? argv[2] : "5";
  GtkWidget *window;
  GtkWidget *outer;
  GtkWidget *inner;
  GtkStyleContext *ctx;
  GdkRGBA color;
  gchar *outer_name;
  guint i;

  /* Must happen before gtk_init(): GTK_MODULES modules are loaded, and
   * therefore gtk_module_init() runs and reads g_get_prgname(), inside
   * gtk_init() itself. */
  g_set_prgname (prgname);

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  outer = gtk_event_box_new ();
  inner = gtk_button_new ();

  outer_name = g_strdup_printf ("launcher-%s", id);
  gtk_widget_set_name (outer, outer_name);
  g_free (outer_name);
  gtk_widget_set_name (inner, "launcher-arrow");

  gtk_container_add (GTK_CONTAINER (outer), inner);
  gtk_container_add (GTK_CONTAINER (window), outer);

  gtk_widget_show_all (window);

  /* Pump the loop briefly so realization and any provider attached by a
   * loaded module have taken effect before we read the computed style. */
  for (i = 0; i < 50 && gtk_events_pending (); i++)
    gtk_main_iteration ();

  ctx = gtk_widget_get_style_context (inner);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (ctx, gtk_style_context_get_state (ctx), &color);
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_print ("bg=%.4f,%.4f,%.4f,%.4f\n", color.red, color.green, color.blue, color.alpha);

  gtk_widget_destroy (window);

  return 0;
}
