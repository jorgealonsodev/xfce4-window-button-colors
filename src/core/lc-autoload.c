/* lc-autoload.c — see lc-autoload.h for the design rationale (D1). This
 * file implements the five pure list transforms (normalize/join/
 * is_enabled/add/remove) AND lc_autoload_set_enabled(), the
 * orchestration layer that reads a shape via LcAutoloadBackend, applies
 * add/remove, and decides the write-back shape per D1's table. Neither
 * half touches xfconf — the vtable's real implementation is Phase 4's
 * src/settings/lc-autoload-xfconf.c.
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

/* ---- set_enabled — orchestration over the vtable, D1's shape table --- */

/* TRUE iff `a` and `b` hold the exact same NULL-terminated sequence of
 * strings, in the same order. A NULL pointer is treated identically to
 * a zero-length array, matching every pure transform's own NULL
 * convention above. This decides LC_AUTOLOAD_NO_CHANGE below: if
 * add()/remove() produced a list that is not byte-for-byte identical to
 * what was read, something changed and a write is owed; if it is
 * identical, D1's "never write when nothing changed" rule applies — the
 * same rule lc_winstore_reconcile() already documents for pruning,
 * carried here to xfconf.
 */
static gboolean
modules_equal (const gchar *const *a, const gchar *const *b)
{
  guint i;

  for (i = 0; ; i++)
    {
      gboolean a_has = (a != NULL && a[i] != NULL);
      gboolean b_has = (b != NULL && b[i] != NULL);

      if (!a_has && !b_has)
        return TRUE;
      if (a_has != b_has)
        return FALSE;
      if (g_strcmp0 (a[i], b[i]) != 0)
        return FALSE;
    }
}

/* See lc-autoload.h for the full contract. Implements D1's table:
 *
 *   read shape      | resulting list | write-back
 *   ----------------|-----------------|------------------------------
 *   ABSENT          | non-empty       | SCALAR (only shape with
 *                    |                 | positive shipped evidence)
 *   SCALAR / ARRAY   | non-empty       | the shape FOUND, unchanged —
 *                    |                 | never converted either way
 *   any              | empty           | remove the key (write ABSENT,
 *                    |                 | `modules` ignored)
 *   any other GType  | —               | never write; UNSUPPORTED_SHAPE
 */
LcAutoloadResult
lc_autoload_set_enabled (const LcAutoloadBackend *backend, gpointer user_data,
                          const gchar *name, gboolean enabled)
{
  LcAutoloadShape shape;
  gchar **current = NULL;
  gchar **updated;
  LcAutoloadResult result;

  g_return_val_if_fail (backend != NULL, LC_AUTOLOAD_READ_FAILED);
  g_return_val_if_fail (name != NULL, LC_AUTOLOAD_READ_FAILED);

  /* 1. Read. The shape is a witness, carried unchanged to write() below
   * (D1). `current` is already the NORMALIZED module list for SCALAR
   * and ARRAY, per the backend contract in lc-autoload.h; ABSENT
   * carries no list at all (backend->read leaves `current` untouched,
   * so it stays at its NULL initializer). */
  if (!backend->read (user_data, &shape, &current))
    return LC_AUTOLOAD_READ_FAILED;

  /* 2. Refuse outright on any shape we do not recognise (D1's
   * "present, any other type" row), rather than guess a conversion —
   * an unrecognised GType is exactly the case where guessing risks
   * destroying a value another module depends on. */
  if (shape != LC_AUTOLOAD_SHAPE_ABSENT &&
      shape != LC_AUTOLOAD_SHAPE_SCALAR &&
      shape != LC_AUTOLOAD_SHAPE_ARRAY)
    {
      g_strfreev (current);
      return LC_AUTOLOAD_UNSUPPORTED_SHAPE;
    }

  /* 3. Apply the pure transform for the requested direction. Every
   * OTHER entry in `current` survives into `updated` unchanged, in
   * order — that guarantee lives in lc_autoload_add()/remove() above,
   * not here; this function only decides WHETHER and HOW to write the
   * result back. */
  updated = enabled ? lc_autoload_add ((const gchar *const *) current, name)
                     : lc_autoload_remove ((const gchar *const *) current, name);

  /* 4. Nothing changed ⇒ never write. Enabling an already-enabled
   * module and disabling an absent one both land here. */
  if (modules_equal ((const gchar *const *) current, (const gchar *const *) updated))
    {
      g_strfreev (current);
      g_strfreev (updated);
      return LC_AUTOLOAD_NO_CHANGE;
    }

  /* 5. Decide the write-back shape per D1's table (see the comment
   * above this function). */
  if (updated[0] == NULL)
    {
      /* The result is empty: remove the key entirely rather than write
       * an empty scalar/array — an empty value is not the same on-disk
       * state as "absent", and D1 is explicit that the key is only
       * removed when it is genuinely empty. */
      result = backend->write (user_data, LC_AUTOLOAD_SHAPE_ABSENT, NULL)
                   ? LC_AUTOLOAD_OK : LC_AUTOLOAD_WRITE_FAILED;
    }
  else
    {
      /* Absent → scalar (the only shape with positive shipped
       * evidence); otherwise the shape ALREADY FOUND, carried
       * unchanged — never SCALAR→ARRAY or ARRAY→SCALAR, because that
       * re-types a key another module may already depend on. */
      LcAutoloadShape write_shape =
          (shape == LC_AUTOLOAD_SHAPE_ABSENT) ? LC_AUTOLOAD_SHAPE_SCALAR : shape;

      result = backend->write (user_data, write_shape, (const gchar *const *) updated)
                   ? LC_AUTOLOAD_OK : LC_AUTOLOAD_WRITE_FAILED;
    }

  g_strfreev (current);
  g_strfreev (updated);
  return result;
}
