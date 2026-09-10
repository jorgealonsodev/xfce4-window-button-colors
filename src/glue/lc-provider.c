/* lc-provider.c — see lc-provider.h.
 *
 * LC_PROVIDER_PRIORITY is supplied at compile time by meson.build as a
 * -DLC_PROVIDER_PRIORITY=<literal> argument, resolved from the
 * `provider_priority` option (design.md D7). The #ifndef fallback below
 * exists only so this file still compiles in isolation (e.g. an IDE
 * syntax pass); the real build always defines it explicitly.
 */
#include "lc-provider.h"

#ifndef LC_PROVIDER_PRIORITY
#define LC_PROVIDER_PRIORITY GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
#endif

/* The module's ONE provider (design.md D6) and the screen it is
 * currently attached to, so lc_provider_detach() can remove it cleanly
 * and a later lc_provider_attach() can replace it instead of stacking a
 * second one. NULL when nothing is attached. */
static GtkCssProvider *lc_provider_active = NULL;
static GdkScreen *lc_provider_active_screen = NULL;

void
lc_provider_attach (GdkScreen *screen, const LcDocument *doc)
{
  GtkCssProvider *provider;
  GError *error = NULL;
  const gchar *css;

  g_return_if_fail (doc != NULL);

  if (screen == NULL)
    {
      g_warning ("xfce4-window-button-colors: no default screen available, "
                 "cannot attach the colour provider");
      return;
    }

  css = lc_document_css (doc);

  provider = gtk_css_provider_new ();
  if (!gtk_css_provider_load_from_data (provider, css != NULL ? css : "", -1, &error))
    {
      /* design.md lifecycle step 7: a GError from CSS parsing discards
       * the rule set with one warning and is never propagated. Whatever
       * was attached before (possibly nothing) is left exactly as it
       * was. */
      g_warning ("xfce4-window-button-colors: discarding colour rules, "
                 "CSS parse failed: %s",
                 error != NULL ? error->message : "(unknown error)");
      g_clear_error (&error);
      g_object_unref (provider);
      return;
    }

  /* D6: one provider, whole-document reload. Detach the previous
   * provider (if any) from its screen before the new one is installed,
   * so a reload never leaves stale rules layered underneath fresh ones. */
  if (lc_provider_active != NULL && lc_provider_active_screen != NULL)
    gtk_style_context_remove_provider_for_screen (lc_provider_active_screen,
                                                   GTK_STYLE_PROVIDER (lc_provider_active));

  gtk_style_context_add_provider_for_screen (screen,
                                              GTK_STYLE_PROVIDER (provider),
                                              LC_PROVIDER_PRIORITY);

  if (lc_provider_active != NULL)
    g_object_unref (lc_provider_active);

  /* provider was returned to us at refcount 1 by gtk_css_provider_new();
   * add_provider_for_screen() above took its own additional reference.
   * We keep the reference we already own instead of taking a fresh one. */
  lc_provider_active = provider;
  lc_provider_active_screen = screen;
}

void
lc_provider_detach (void)
{
  if (lc_provider_active == NULL)
    return;

  if (lc_provider_active_screen != NULL)
    gtk_style_context_remove_provider_for_screen (lc_provider_active_screen,
                                                   GTK_STYLE_PROVIDER (lc_provider_active));

  g_object_unref (lc_provider_active);
  lc_provider_active = NULL;
  lc_provider_active_screen = NULL;
}
