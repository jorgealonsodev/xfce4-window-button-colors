/* idempotency-probe.c — throwaway, non-panel GTK3 process that directly
 * exercises gtk_module_init()/gtk_module_exit() idempotency (tasks.md
 * 3.3, design.md lifecycle step 2), rather than inferring it from a
 * single GTK_MODULES-triggered load. NOT installed, never touches the
 * real panel or any xfconf key.
 *
 * It replicates exactly what GTK itself does when GTK_MODULES names a
 * module (dlopen the .so, dlsym "gtk_module_init"/"gtk_module_exit",
 * call them with the process's own argc/argv) via GModule, but keeps
 * manual control so init and exit can each be called twice in one
 * process and the effect observed in between.
 *
 * Usage:
 *   HOME=/some/temp/xdg/home \
 *   ./idempotency-probe /abs/path/to/libxfce4-window-button-colors.so
 *
 * Always runs with prgname forced to "xfce4-panel" (the guard is not
 * what this probe is about — 3.6's guard-inert proof covers that
 * separately). Prints one "bg=..." line after each call so the sequence
 * (attach, no-op, detach, no-op) is directly observable:
 *
 *   after 1st init : coloured   (bg alpha > 0)
 *   after 2nd init : coloured, unchanged (idempotent no-op)
 *   after 1st exit : unstyled again (bg alpha == 0)
 *   after 2nd exit : unstyled, unchanged (idempotent no-op)
 *
 * Exit code is always 0 unless the module or its two symbols cannot be
 * loaded at all, which is a harness setup failure, not a module bug.
 */
#include <gtk/gtk.h>
#include <gmodule.h>

typedef void (*lc_module_init_fn) (gint *argc, gchar ***argv);
typedef void (*lc_module_exit_fn) (void);

/* Builds a fresh "#launcher-5 #launcher-arrow"-shaped hierarchy, realizes
 * it and prints the inner widget's resolved background colour, then
 * tears it down again. A FRESH GtkStyleContext is used every time
 * (rather than re-querying one built before the provider was
 * attached/detached) so this sidesteps any style-cache invalidation
 * timing question entirely: a brand new context always resolves against
 * whatever providers are attached to the screen right now. */
static void
sample_bg (const gchar *label)
{
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *outer = gtk_event_box_new ();
  GtkWidget *inner = gtk_button_new ();
  GtkStyleContext *ctx;
  GdkRGBA color;
  guint i;

  gtk_widget_set_name (outer, "launcher-5");
  gtk_widget_set_name (inner, "launcher-arrow");
  gtk_container_add (GTK_CONTAINER (outer), inner);
  gtk_container_add (GTK_CONTAINER (window), outer);
  gtk_widget_show_all (window);

  for (i = 0; i < 50; i++)
    g_main_context_iteration (NULL, FALSE);

  ctx = gtk_widget_get_style_context (inner);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (ctx, gtk_style_context_get_state (ctx), &color);
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_print ("%-16s bg=%.4f,%.4f,%.4f,%.4f\n", label, color.red, color.green, color.blue, color.alpha);

  gtk_widget_destroy (window);
}

int
main (int argc, char **argv)
{
  GModule *module;
  gpointer init_sym = NULL;
  gpointer exit_sym = NULL;
  lc_module_init_fn module_init;
  lc_module_exit_fn module_exit;

  if (argc < 2)
    {
      g_printerr ("usage: %s <path-to-module.so>\n", argv[0]);
      return 1;
    }

  g_set_prgname ("xfce4-panel");
  gtk_init (&argc, &argv);

  module = g_module_open (argv[1], G_MODULE_BIND_LOCAL);
  if (module == NULL)
    {
      g_printerr ("g_module_open failed: %s\n", g_module_error ());
      return 1;
    }

  if (!g_module_symbol (module, "gtk_module_init", &init_sym) ||
      !g_module_symbol (module, "gtk_module_exit", &exit_sym))
    {
      g_printerr ("g_module_symbol failed: %s\n", g_module_error ());
      return 1;
    }

  module_init = (lc_module_init_fn) init_sym;
  module_exit = (lc_module_exit_fn) exit_sym;

  sample_bg ("before any init:");

  module_init (&argc, &argv);
  sample_bg ("after 1st init:");

  module_init (&argc, &argv); /* must be a safe no-op */
  sample_bg ("after 2nd init:");

  module_exit ();
  sample_bg ("after 1st exit:");

  module_exit (); /* must be a safe no-op */
  sample_bg ("after 2nd exit:");

  g_module_close (module);

  g_print ("idempotency-probe: completed without crashing\n");

  return 0;
}
