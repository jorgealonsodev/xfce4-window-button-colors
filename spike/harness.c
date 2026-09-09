/* spike/harness.c — Slice 2 (H0-b) standalone GTK3 evidence harness.
 *
 * THROWAWAY SPIKE CODE. Not part of any install target, not linked into
 * lc-core, not wired into meson.build. Its output is empirical evidence
 * recorded in docs/spike-findings.md, not production code (design.md
 * "Open Questions").
 *
 * This harness never touches xfconf, never loads into a running process
 * via /Gtk/Modules, and never disturbs the live panel. It is an ordinary
 * standalone GTK3 program that reproduces, in its own widgets, the exact
 * structure and CSS-provider situation xfce4-panel's launcher plugin uses,
 * then answers three open questions empirically:
 *
 *   Q1 — which GtkStyleProvider priority (and scope: screen vs widget)
 *        actually wins against the panel's own widget-level
 *        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION rule on the button.
 *   Q2 — does our rule visibly paint, and does theme hover survive it,
 *        captured as PNG screenshots via GtkOffscreenWindow.
 *   Q3 — (harness evidence only, NOT a live-panel confirmation) the
 *        relative "map" emission order for this exact container/child
 *        widget shape, to sanity-check design.md D4's "the plugin's own
 *        map fires, no ancestor walk needed" assumption.
 *
 * Reproduced structure (verified against xfce4-panel 4.16.0, 4.18.0 and
 * master's launcher_plugin_init()):
 *   - an outer container named "launcher-13" standing in for the
 *     XfcePanelPlugin;
 *   - a GtkButton inside it whose name is set to "launcher-button" and
 *     then immediately OVERWRITTEN to "launcher-arrow" — this exact
 *     upstream naming bug is reproduced verbatim, not fixed;
 *   - a widget-level GtkCssProvider on the button's own style context at
 *     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION loading
 *     "#launcher-arrow { min-height: 0; min-width: 0; }" — the panel's own
 *     competing provider that our provider must beat.
 *
 * Build (no meson target — kept out of the install tree per task 2.7):
 *   gcc -std=c11 -Wall -Wextra $(pkg-config --cflags gtk+-3.0) \
 *       -o spike/harness spike/harness.c $(pkg-config --libs gtk+-3.0) -lm
 *
 * Run:
 *   ./spike/harness                 # system default theme (reads live
 *                                    # xsettings, changes nothing)
 *   GTK_THEME=Adwaita ./spike/harness  # Adwaita, this process only
 */

#include <gtk/gtk.h>
#include <cairo.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define OUR_RULE \
  "#launcher-13 #launcher-arrow { " \
  "background-color: rgba(233,30,140,0.85); border-radius: 6px; }"
#define PANEL_RULE \
  "#launcher-arrow { min-height: 0; min-width: 0; }"

typedef enum
{
  ATTACH_SCREEN,
  ATTACH_WIDGET
} AttachScope;

typedef struct
{
  GtkWidget *offscreen;
  GtkWidget *outer;
  GtkWidget *button;
} Widgets;

static const char *out_dir = "spike/out";
static char theme_tag[128] = "unknown";
static int map_hook_events_logged = 0;

/* ---- Q3: global "map" emission hook, installed once for the whole
 * harness lifetime. Filters to only the two widgets we care about so
 * repeated scenario runs do not flood the log once the finding is
 * established. ------------------------------------------------------- */
static gboolean
on_widget_map (GSignalInvocationHint *ihint, guint n_param_values,
                const GValue *param_values, gpointer user_data)
{
  (void) ihint;
  (void) n_param_values;
  (void) user_data;

  GObject *obj = g_value_get_object (&param_values[0]);
  if (!GTK_IS_WIDGET (obj))
    return TRUE;

  const char *name = gtk_widget_get_name (GTK_WIDGET (obj));
  if (g_strcmp0 (name, "launcher-13") != 0 &&
      g_strcmp0 (name, "launcher-arrow") != 0)
    return TRUE; /* not one of our two widgets of interest */

  if (map_hook_events_logged < 8) /* cap noise across repeated scenarios */
    {
      printf ("[Q3][harness-only, NOT confirmed against real panel] "
              "map fired: type=%s name=\"%s\" addr=%p\n",
              G_OBJECT_TYPE_NAME (obj), name, (void *) obj);
      map_hook_events_logged++;
    }

  return TRUE; /* keep the hook installed */
}

static void
pump (void)
{
  int guard = 0;
  while (gtk_events_pending () && guard < 200)
    {
      gtk_main_iteration ();
      guard++;
    }
}

static Widgets
build_widgets (void)
{
  Widgets w;

  w.offscreen = gtk_offscreen_window_new ();
  w.outer = gtk_event_box_new ();
  gtk_widget_set_name (w.outer, "launcher-13");

  w.button = gtk_button_new_with_label ("L");
  /* Reproduces launcher_plugin_init()'s exact naming sequence: set to
   * "launcher-button" first, then immediately overwritten. */
  gtk_widget_set_name (w.button, "launcher-button");
  gtk_widget_set_name (w.button, "launcher-arrow");

  gtk_container_add (GTK_CONTAINER (w.outer), w.button);
  gtk_container_add (GTK_CONTAINER (w.offscreen), w.outer);

  /* The panel's own widget-level provider — always present, the
   * competing provider our rule must beat. */
  GtkCssProvider *panel_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (panel_provider, PANEL_RULE, -1, NULL);
  gtk_style_context_add_provider (
      gtk_widget_get_style_context (w.button),
      GTK_STYLE_PROVIDER (panel_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (panel_provider);

  return w;
}

static void
realize_widgets (Widgets *w)
{
  gtk_widget_show_all (w->offscreen);
  pump ();
  pump ();
}

static void
sample_pixel (cairo_surface_t *surface, int x, int y,
              double *r, double *g, double *b, double *a)
{
  cairo_surface_flush (surface);
  unsigned char *data = cairo_image_surface_get_data (surface);
  int stride = cairo_image_surface_get_stride (surface);
  unsigned char *p = data + y * stride + x * 4;
  double alpha_byte = p[3];
  *a = alpha_byte / 255.0;
  if (alpha_byte > 0)
    {
      *b = p[0] * 255.0 / alpha_byte;
      *g = p[1] * 255.0 / alpha_byte;
      *r = p[2] * 255.0 / alpha_byte;
    }
  else
    {
      *b = p[0];
      *g = p[1];
      *r = p[2];
    }
}

static void
render_button_background (GtkWidget *button,
                           double *r, double *g, double *b, double *a)
{
  GtkAllocation alloc;
  gtk_widget_get_allocation (button, &alloc);
  int w = alloc.width > 0 ? alloc.width : 40;
  int h = alloc.height > 0 ? alloc.height : 24;

  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create (surface);
  gtk_render_background (gtk_widget_get_style_context (button), cr, 0, 0, w, h);
  sample_pixel (surface, w / 2, h / 2, r, g, b, a);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

static gboolean
save_offscreen_png (GtkWidget *offscreen, const char *filename)
{
  GtkAllocation alloc;
  gtk_widget_get_allocation (offscreen, &alloc);
  int w = alloc.width > 0 ? alloc.width : 200;
  int h = alloc.height > 0 ? alloc.height : 100;

  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  cairo_t *cr = cairo_create (surface);
  gtk_widget_draw (offscreen, cr);
  char path[512];
  g_snprintf (path, sizeof (path), "%s/%s", out_dir, filename);
  cairo_status_t status = cairo_surface_write_to_png (surface, path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  if (status != CAIRO_STATUS_SUCCESS)
    {
      fprintf (stderr, "  ! failed to write %s: %s\n", path,
               cairo_status_to_string (status));
      return FALSE;
    }
  printf ("  wrote %s (%dx%d)\n", path, w, h);
  return TRUE;
}

static gboolean
looks_like_our_pink (double r, double g, double b, double a)
{
  return a > 0.5 && fabs (r - 233) < 8 && fabs (g - 30) < 8 && fabs (b - 140) < 8;
}

/* ---- Q1: priority/scope comparison scenario -------------------------- */
static gboolean
run_priority_scenario (const char *label, guint our_priority, AttachScope scope)
{
  Widgets w = build_widgets ();

  GtkCssProvider *ours = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (ours, OUR_RULE, -1, NULL);

  GdkScreen *screen = gtk_widget_get_screen (w.offscreen);
  if (scope == ATTACH_SCREEN)
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours), our_priority);
  else
    gtk_style_context_add_provider (gtk_widget_get_style_context (w.button),
                                     GTK_STYLE_PROVIDER (ours), our_priority);

  realize_widgets (&w);

  double r, g, b, a;
  render_button_background (w.button, &r, &g, &b, &a);
  gboolean ours_won = looks_like_our_pink (r, g, b, a);

  printf ("[Q1][%s] resolved background: rgba(%.0f,%.0f,%.0f,%.2f) -> %s\n",
          label, r, g, b, a,
          ours_won ? "OUR provider WINS" : "panel/theme default wins (our rule lost)");

  if (scope == ATTACH_SCREEN)
    gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours));
  else
    gtk_style_context_remove_provider (gtk_widget_get_style_context (w.button),
                                        GTK_STYLE_PROVIDER (ours));
  g_object_unref (ours);
  gtk_widget_destroy (w.offscreen);

  return ours_won;
}

/* ---- Q1b: genuine same-property conflict at EQUAL priority -----------
 * design.md D7's rationale claims "widget-level providers outrank
 * screen-level ones at equal priority". The panel's real min-size-only
 * rule never conflicts with our background-color property, so it cannot
 * test that claim. This scenario adds a second, decoy widget-level
 * provider directly on the button that DOES set background-color, at the
 * SAME priority as our screen-level provider, so the two genuinely
 * compete over the same property. */
#define DECOY_RULE "#launcher-arrow { background-color: rgba(80,80,80,1.0); }"

static gboolean
run_equal_priority_conflict_scenario (void)
{
  Widgets w = build_widgets ();

  GtkCssProvider *decoy = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (decoy, DECOY_RULE, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (w.button),
                                   GTK_STYLE_PROVIDER (decoy),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GtkCssProvider *ours = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (ours, OUR_RULE, -1, NULL);
  GdkScreen *screen = gtk_widget_get_screen (w.offscreen);
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  realize_widgets (&w);

  double r, g, b, a;
  render_button_background (w.button, &r, &g, &b, &a);
  gboolean ours_won = looks_like_our_pink (r, g, b, a);

  printf ("[Q1b][screen-vs-widget SAME property, EQUAL priority(600)] "
          "resolved background: rgba(%.0f,%.0f,%.0f,%.2f) -> %s\n",
          r, g, b, a,
          ours_won ? "our SCREEN provider wins (refutes widget>screen-at-equal-priority claim)"
                    : "the WIDGET-level decoy wins (confirms D7's widget>screen-at-equal-priority claim)");

  gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours));
  g_object_unref (ours);
  g_object_unref (decoy);
  gtk_widget_destroy (w.offscreen);

  return ours_won;
}

/* ---- Diagnostic: explain a Q1 LOSE by testing whether the active
 * theme paints an opaque background-image (e.g. Adwaita's button
 * gradient) that occludes a background-color-only override. This rule
 * is NOT the one specified for Q1 — it is a supplementary probe run
 * only to interpret an unexpected loss, never used for the Q1 verdict
 * itself. ------------------------------------------------------------- */
#define OUR_RULE_NO_BG_IMAGE \
  "#launcher-13 #launcher-arrow { " \
  "background-color: rgba(233,30,140,0.85); background-image: none; " \
  "border-radius: 6px; }"

static gboolean
run_background_image_diagnostic (void)
{
  Widgets w = build_widgets ();
  GtkCssProvider *ours = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (ours, OUR_RULE_NO_BG_IMAGE, -1, NULL);
  GdkScreen *screen = gtk_widget_get_screen (w.offscreen);
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  realize_widgets (&w);

  double r, g, b, a;
  render_button_background (w.button, &r, &g, &b, &a);
  gboolean ours_won = looks_like_our_pink (r, g, b, a);
  printf ("[Q1-diagnostic][screen/APPLICATION + background-image:none] "
          "resolved background: rgba(%.0f,%.0f,%.0f,%.2f) -> %s\n",
          r, g, b, a, ours_won ? "OUR provider WINS once background-image is cleared"
                                : "still lost even with background-image:none");

  gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours));
  g_object_unref (ours);
  gtk_widget_destroy (w.offscreen);
  return ours_won;
}

/* ---- Q2: paint + hover screenshot capture ---------------------------- */
static void
run_paint_and_hover (guint winning_priority, AttachScope winning_scope,
                      const char *priority_label)
{
  /* Baseline: panel provider only, no colour rule. */
  {
    Widgets w = build_widgets ();
    realize_widgets (&w);
    char fn[256];
    g_snprintf (fn, sizeof (fn), "baseline-%s.png", theme_tag);
    save_offscreen_png (w.offscreen, fn);
    double r, g, b, a;
    render_button_background (w.button, &r, &g, &b, &a);
    printf ("[Q2][%s] baseline background: rgba(%.0f,%.0f,%.0f,%.2f)\n",
            theme_tag, r, g, b, a);
    gtk_widget_destroy (w.offscreen);
  }

  /* Painted at the winning priority/scope. */
  {
    Widgets w = build_widgets ();
    GtkCssProvider *ours = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (ours, OUR_RULE, -1, NULL);
    GdkScreen *screen = gtk_widget_get_screen (w.offscreen);
    if (winning_scope == ATTACH_SCREEN)
      gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours), winning_priority);
    else
      gtk_style_context_add_provider (gtk_widget_get_style_context (w.button),
                                       GTK_STYLE_PROVIDER (ours), winning_priority);
    realize_widgets (&w);

    char fn[256];
    g_snprintf (fn, sizeof (fn), "painted-%s-%s.png", theme_tag, priority_label);
    save_offscreen_png (w.offscreen, fn);
    double r, g, b, a;
    render_button_background (w.button, &r, &g, &b, &a);
    printf ("[Q2][%s] painted (%s) background: rgba(%.0f,%.0f,%.0f,%.2f)\n",
            theme_tag, priority_label, r, g, b, a);

    /* Hover: force GTK_STATE_FLAG_PRELIGHT and re-render/re-capture. */
    gtk_widget_set_state_flags (w.button, GTK_STATE_FLAG_PRELIGHT, FALSE);
    pump ();
    g_snprintf (fn, sizeof (fn), "hover-%s-%s.png", theme_tag, priority_label);
    save_offscreen_png (w.offscreen, fn);
    render_button_background (w.button, &r, &g, &b, &a);
    printf ("[Q2][%s] hover (%s) background: rgba(%.0f,%.0f,%.0f,%.2f)\n",
            theme_tag, priority_label, r, g, b, a);

    if (winning_scope == ATTACH_SCREEN)
      gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (ours));
    else
      gtk_style_context_remove_provider (gtk_widget_get_style_context (w.button),
                                          GTK_STYLE_PROVIDER (ours));
    g_object_unref (ours);
    gtk_widget_destroy (w.offscreen);
  }
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  /* gtk_init() calls setlocale(LC_ALL, "") internally, which would make
   * the printed evidence numbers below use the process locale's decimal
   * separator (e.g. es_ES uses ','). Force LC_NUMERIC back to "C" AFTER
   * gtk_init() so "rgba(...)" values stay in a stable, unambiguous
   * decimal-point format regardless of the user's locale. */
  setlocale (LC_NUMERIC, "C");

  /* GTK_THEME overrides the actual rendered CSS but does not always show
   * up in the "gtk-theme-name" GtkSettings property read back afterwards
   * (observed: the property still reports the xsettings-provided name
   * even though the rendered colours clearly come from the GTK_THEME
   * theme). Prefer the environment variable for tagging/reporting when
   * set, since it reflects what this process actually requested; fall
   * back to the live GtkSettings property (the system default) when
   * GTK_THEME is unset. */
  const char *env_theme = g_getenv ("GTK_THEME");
  gchar *theme_name = NULL;
  if (env_theme != NULL && env_theme[0] != '\0')
    {
      theme_name = g_strdup (env_theme);
    }
  else
    {
      GtkSettings *settings = gtk_settings_get_default ();
      g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
    }
  g_strlcpy (theme_tag, theme_name ? theme_name : "unknown", sizeof (theme_tag));
  /* Filenames must not contain spaces or the ":variant" separator. */
  for (char *p = theme_tag; *p; p++)
    if (*p == ' ' || *p == ':') *p = '-';
  printf ("=== spike/harness: GTK theme in effect for this process: %s "
          "(source: %s) ===\n",
          theme_name, env_theme != NULL && env_theme[0] != '\0' ? "GTK_THEME env" : "live GtkSettings");
  g_free (theme_name);

  g_mkdir_with_parents (out_dir, 0755);

  /* Q3 hook installed for the whole run. */
  guint map_signal_id = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  g_signal_add_emission_hook (map_signal_id, 0, on_widget_map, NULL, NULL);

  /* ---- Q1: priority + scope matrix ---- */
  printf ("\n--- Q1: provider priority / scope comparison ---\n");
  gboolean screen_app = run_priority_scenario ("screen/APPLICATION(600)",
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION,
                                                ATTACH_SCREEN);
  gboolean screen_user = run_priority_scenario ("screen/USER(800)",
                                                 GTK_STYLE_PROVIDER_PRIORITY_USER,
                                                 ATTACH_SCREEN);
  gboolean widget_app = run_priority_scenario ("widget/APPLICATION(600)",
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION,
                                                ATTACH_WIDGET);
  gboolean widget_user = run_priority_scenario ("widget/USER(800)",
                                                 GTK_STYLE_PROVIDER_PRIORITY_USER,
                                                 ATTACH_WIDGET);

  printf ("\n[Q1][VERDICT] screen/APPLICATION=%s screen/USER=%s "
          "widget/APPLICATION=%s widget/USER=%s\n",
          screen_app ? "WIN" : "LOSE", screen_user ? "WIN" : "LOSE",
          widget_app ? "WIN" : "LOSE", widget_user ? "WIN" : "LOSE");

  gboolean equal_priority_screen_wins = run_equal_priority_conflict_scenario ();
  printf ("[Q1b][VERDICT] screen-level provider at equal(600) priority "
          "vs a same-property widget-level decoy: %s\n",
          equal_priority_screen_wins ? "SCREEN WINS" : "WIDGET WINS");

  /* Determine the winning screen-level priority for the Q2 capture,
   * matching production's screen-level default scope (design.md D7). */
  guint winning_priority;
  const char *priority_label;
  if (screen_app)
    {
      winning_priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
      priority_label = "application";
    }
  else if (screen_user)
    {
      winning_priority = GTK_STYLE_PROVIDER_PRIORITY_USER;
      priority_label = "user";
    }
  else
    {
      /* Neither screen-level priority beat the widget-level panel rule;
       * fall back to USER as the closest candidate and let
       * docs/spike-findings.md record the anomaly explicitly. Run the
       * background-image diagnostic to interpret WHY. */
      winning_priority = GTK_STYLE_PROVIDER_PRIORITY_USER;
      priority_label = "user";
      printf ("[Q1][WARNING] no screen-level priority beat the widget-level "
              "panel rule; Q2 capture falls back to USER for illustration.\n");
      run_background_image_diagnostic ();
    }

  /* ---- Q2: paint + hover screenshots ---- */
  printf ("\n--- Q2: paint + hover screenshots (theme=%s) ---\n", theme_tag);
  run_paint_and_hover (winning_priority, ATTACH_SCREEN, priority_label);

  printf ("\n=== spike/harness: done for theme %s ===\n", theme_tag);
  return 0;
}
