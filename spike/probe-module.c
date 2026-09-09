/* probe-module.c — throwaway diagnostic module for the H0-b live checklist.
 *
 * NOT production code. It is never installed and never linked into the real
 * module. It exists to answer two questions that the standalone harness
 * could not:
 *
 *   1. Design D4 assumes the XfcePanelPlugin object itself emits ::map, so
 *      no ancestor walk is needed. The harness saw child-before-container
 *      ordering, which contradicts that rationale. Only the real panel can
 *      settle it.
 *   2. Whether a single base-rule `background-image: none` also suppresses
 *      the theme image in :hover. The harness probe ran only in the
 *      non-hover state, so there is no evidence either way.
 *
 * Safety rules this file obeys, because it runs inside the user's live
 * panel (RNF-6): no g_error, no g_assert, no abort, no exit on any path.
 * Every failure warns and continues. The emission hook always returns TRUE.
 * It writes one log file and touches nothing else — no xfconf, no config.
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>

#define PROBE_LOG "/tmp/xlc-probe.log"

/* Seconds after load before each stage runs. Generous, so the panel is fully
 * built and every launcher has mapped before we look. */
#define STAGE_COLOUR_DELAY  4
#define STAGE_HOVER_DELAY   9

G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);

static FILE      *log_fp;
static GtkWidget *found_button;      /* the inner clickable button, named launcher-arrow */
static gint       found_id = -1;     /* the plugin's unique id, parsed from launcher-<id> */
static guint      map_seq;

static void
probe_log (const char *fmt, ...)
{
  va_list args;

  if (log_fp == NULL)
    return;

  va_start (args, fmt);
  vfprintf (log_fp, fmt, args);
  va_end (args);
  fputc ('\n', log_fp);
  fflush (log_fp);
}

/* Parses "launcher-13" into 13. Returns -1 for anything else, including
 * "launcher-arrow" and "launcher-button". */
static gint
launcher_id_from_name (const char *name)
{
  const char *suffix;
  char       *end;
  gint64      value;

  if (name == NULL || !g_str_has_prefix (name, "launcher-"))
    return -1;

  suffix = name + strlen ("launcher-");
  if (*suffix == '\0' || !g_ascii_isdigit (*suffix))
    return -1;

  value = g_ascii_strtoll (suffix, &end, 10);
  if (end == NULL || *end != '\0' || value < 0 || value > G_MAXINT)
    return -1;

  return (gint) value;
}

static gboolean
map_hook (GSignalInvocationHint *ihint,
          guint                  n_param_values,
          const GValue          *param_values,
          gpointer               user_data)
{
  GtkWidget  *widget;
  const char *name;
  const char *type_name;
  gint        id;

  (void) ihint;
  (void) user_data;

  /* Defensive: the hook fires for every widget in the process. Anything
   * unexpected must be ignored silently, never fatal. */
  if (n_param_values < 1)
    return TRUE;

  widget = g_value_get_object (&param_values[0]);
  if (!GTK_IS_WIDGET (widget))
    return TRUE;

  name = gtk_widget_get_name (widget);
  if (name == NULL || !g_str_has_prefix (name, "launcher-"))
    return TRUE;

  type_name = G_OBJECT_TYPE_NAME (widget);
  id = launcher_id_from_name (name);

  map_seq++;
  probe_log ("[map %03u] name=%-18s type=%-28s %s",
             map_seq,
             name,
             type_name != NULL ? type_name : "(null)",
             id >= 0 ? "PLUGIN OBJECT (D4 assumption holds for this one)"
                     : "inner widget");

  /* Remember the first inner button we see, and the first plugin id. That
   * pair is what the colour stage needs. */
  if (id >= 0 && found_id < 0)
    found_id = id;

  if (id < 0 && g_strcmp0 (name, "launcher-arrow") == 0 && found_button == NULL)
    found_button = widget;

  return TRUE;
}

static gboolean
stage_apply_colour (gpointer user_data)
{
  GtkCssProvider *provider;
  GdkScreen      *screen;
  GError         *error = NULL;
  gchar          *css;

  (void) user_data;

  if (found_id < 0)
    {
      probe_log ("");
      probe_log ("STAGE colour: SKIPPED — no launcher-<id> widget ever mapped.");
      probe_log ("  That is itself the D4 answer: the plugin object does not emit ::map.");
      return G_SOURCE_REMOVE;
    }

  /* The real three-rule template from design D17. */
  css = g_strdup_printf
    ("#launcher-%d #launcher-arrow {\n"
     "  background-image: none;\n"
     "  background-color: rgba(233,30,140,0.85);\n"
     "  border-radius: 6px;\n"
     "}\n"
     "#launcher-%d #launcher-arrow:hover {\n"
     "  background-color: rgba(237,71,161,0.88);\n"
     "}\n"
     "#launcher-%d #launcher-arrow:active {\n"
     "  background-color: rgba(158,20,95,0.90);\n"
     "}\n",
     found_id, found_id, found_id);

  provider = gtk_css_provider_new ();
  if (!gtk_css_provider_load_from_data (provider, css, -1, &error))
    {
      probe_log ("STAGE colour: CSS parse failed: %s",
                 error != NULL ? error->message : "(no message)");
      g_clear_error (&error);
      g_free (css);
      g_object_unref (provider);
      return G_SOURCE_REMOVE;
    }

  screen = gdk_screen_get_default ();
  if (screen == NULL)
    {
      probe_log ("STAGE colour: no default screen, cannot attach provider");
      g_free (css);
      g_object_unref (provider);
      return G_SOURCE_REMOVE;
    }

  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  probe_log ("");
  probe_log ("STAGE colour: applied to launcher-%d at APPLICATION (600), screen scope.",
             found_id);
  probe_log ("  Take a screenshot NOW — this is the base (non-hover) state.");

  g_free (css);
  return G_SOURCE_REMOVE;
}

static gboolean
stage_force_hover (gpointer user_data)
{
  (void) user_data;

  if (found_button == NULL || !GTK_IS_WIDGET (found_button))
    {
      probe_log ("");
      probe_log ("STAGE hover: SKIPPED — never captured a launcher-arrow widget.");
      return G_SOURCE_REMOVE;
    }

  gtk_widget_set_state_flags (found_button, GTK_STATE_FLAG_PRELIGHT, FALSE);
  gtk_widget_queue_draw (found_button);

  probe_log ("");
  probe_log ("STAGE hover: forced PRELIGHT on the launcher-arrow widget.");
  probe_log ("  Take a screenshot NOW — this answers whether the base rule's");
  probe_log ("  background-image: none also suppresses the theme image on :hover.");

  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
  const char *prgname;
  guint       map_signal;

  (void) argc;
  (void) argv;

  /* RNF-6 guard: do nothing at all in any process that is not the panel. */
  prgname = g_get_prgname ();
  if (g_strcmp0 (prgname, "xfce4-panel") != 0)
    return;

  log_fp = fopen (PROBE_LOG, "w");
  if (log_fp == NULL)
    return;   /* No log, no probe. Never fatal. */

  probe_log ("xlc probe loaded in prgname=%s", prgname);
  probe_log ("gtk %u.%u.%u  theme=%s",
             gtk_get_major_version (), gtk_get_minor_version (),
             gtk_get_micro_version (),
             g_getenv ("GTK_THEME") != NULL ? g_getenv ("GTK_THEME") : "(session default)");
  probe_log ("");
  probe_log ("Watching GtkWidget::map for launcher-* widgets.");
  probe_log ("D4 asks whether the plugin object itself (named launcher-<id>) emits map,");
  probe_log ("or only its inner children do. Order below is emission order.");
  probe_log ("");

  map_signal = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  if (map_signal == 0)
    {
      probe_log ("FATAL(soft): g_signal_lookup(\"map\", GTK_TYPE_WIDGET) returned 0.");
      return;
    }

  g_signal_add_emission_hook (map_signal, 0, map_hook, NULL, NULL);

  g_timeout_add_seconds (STAGE_COLOUR_DELAY, stage_apply_colour, NULL);
  g_timeout_add_seconds (STAGE_HOVER_DELAY, stage_force_hover, NULL);
}
