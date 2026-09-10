/* lc-autoload.c — see lc-autoload.h for the design rationale (D1) and
 * the Phase 2 / Phase 3 split. This file implements ONLY the five pure
 * list transforms declared there; lc_autoload_set_enabled() is declared
 * in the header but has NO BODY here — Phase 3 adds it in this same
 * file, alongside LcAutoloadShape's actual read/write orchestration and
 * D1's full table.
 */
#include "lc-autoload.h"

/* ---- normalize ---------------------------------------------------------
 *
 * See lc-autoload.h for why the ':' split exists: `/Gtk/Modules` is
 * GTK's own colon-separated `gtk-modules` value, so a scalar like
 * "canberra-gtk-module:xfce4-window-button-colors" is two entries, not
 * one. Skipping this split is the exact bug this function exists to
 * rule out.
 */
gchar **
lc_autoload_normalize (const gchar *scalar)
{
  GPtrArray *out = g_ptr_array_new ();

  if (scalar != NULL && scalar[0] != '\0')
    {
      gchar **pieces = g_strsplit (scalar, ":", -1);
      guint i;

      for (i = 0; pieces[i] != NULL; i++)
        {
          gchar *trimmed = g_strdup (pieces[i]);

          g_strstrip (trimmed);
          if (trimmed[0] != '\0')
            g_ptr_array_add (out, trimmed);
          else
            g_free (trimmed);
        }

      g_strfreev (pieces);
    }

  /* NULL-terminate: the whole point of this contract is "never NULL,
   * always a valid gchar** a caller can g_strfreev()", even for an
   * empty result. */
  g_ptr_array_add (out, NULL);

  return (gchar **) g_ptr_array_free (out, FALSE);
}

/* ---- join ----------------------------------------------------------------
 *
 * g_strjoinv() already handles a zero-length array correctly (joins to
 * ""), and requires a non-NULL, NULL-terminated array — empty_modules
 * below exists only to satisfy that requirement for the "modules ==
 * NULL" case this function's own contract additionally accepts.
 */
gchar *
lc_autoload_join (const gchar *const *modules)
{
  static const gchar *empty_modules[] = { NULL };

  if (modules == NULL)
    modules = empty_modules;

  /* g_strjoinv()'s signature takes a non-const gchar **, but it only
   * reads the array and the strings it points to; this function never
   * mutates `modules`. */
  return g_strjoinv (":", (gchar **) modules);
}

/* ---- is_enabled ----------------------------------------------------------
 *
 * Exact match ONLY — see lc-autoload.h. A prefix/substring compare here
 * would let "xfce4-window-button-colors-extra" read as our module being
 * enabled, which would make a later disable action target the wrong
 * entry.
 */
gboolean
lc_autoload_is_enabled (const gchar *const *modules, const gchar *name)
{
  guint i;

  if (modules == NULL || name == NULL)
    return FALSE;

  for (i = 0; modules[i] != NULL; i++)
    {
      if (g_strcmp0 (modules[i], name) == 0)
        return TRUE;
    }

  return FALSE;
}

/* ---- add -------------------------------------------------------------
 *
 * Idempotent by construction: the exact-match lc_autoload_is_enabled()
 * check below is the only thing deciding whether `name` is appended, so
 * calling this twice in a row hits the same check both times and never
 * appends a second copy.
 */
gchar **
lc_autoload_add (const gchar *const *modules, const gchar *name)
{
  GPtrArray *out;

  g_return_val_if_fail (name != NULL, NULL);

  out = g_ptr_array_new ();

  if (modules != NULL)
    {
      guint i;

      for (i = 0; modules[i] != NULL; i++)
        g_ptr_array_add (out, g_strdup (modules[i]));
    }

  if (!lc_autoload_is_enabled (modules, name))
    g_ptr_array_add (out, g_strdup (name));

  g_ptr_array_add (out, NULL);

  return (gchar **) g_ptr_array_free (out, FALSE);
}

/* ---- remove ----------------------------------------------------------
 *
 * Order-preserving, and preserves every OTHER entry byte-for-byte: this
 * is the function D1 relies on to guarantee a third-party module's
 * autoload survives disabling ours, regardless of shape. Absence of
 * `name` is a no-op, never an error: the loop below simply copies every
 * entry through unchanged.
 */
gchar **
lc_autoload_remove (const gchar *const *modules, const gchar *name)
{
  GPtrArray *out;

  g_return_val_if_fail (name != NULL, NULL);

  out = g_ptr_array_new ();

  if (modules != NULL)
    {
      guint i;

      for (i = 0; modules[i] != NULL; i++)
        {
          if (g_strcmp0 (modules[i], name) != 0)
            g_ptr_array_add (out, g_strdup (modules[i]));
        }
    }

  g_ptr_array_add (out, NULL);

  return (gchar **) g_ptr_array_free (out, FALSE);
}
