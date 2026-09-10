/* lc-autoload-xfconf.c — see lc-autoload-xfconf.h. The sole real
 * `LcAutoloadBackend` implementation (design.md D1). All shape
 * detection, reading and writing against the "xsettings" channel's
 * `/Gtk/Modules` property lives in this file and nowhere else.
 *
 * `read()` and `write()` are deliberately thin: every actual decision
 * about WHAT shape to write back, WHETHER to write at all, and HOW to
 * preserve a third-party entry already lives in the pure, tested
 * `lc_autoload_set_enabled()` (src/core/lc-autoload.c). This file's only
 * job is to translate that orchestration's two calls into the matching
 * xfconf calls, and to translate xfconf's raw GValue back into the
 * normalized `gchar **` shape lc-autoload.h's backend contract requires.
 */
#include "lc-autoload-xfconf.h"

/* ---- read helpers ------------------------------------------------------- */

/* xfconf represents an array-typed property as a GValue holding a
 * G_TYPE_PTR_ARRAY of `GValue *` elements (the same GPtrArray
 * `xfconf_channel_get_arrayv()`/`xfconf_channel_set_arrayv()` operate
 * on). `/Gtk/Modules` as an array is always an array of strings in
 * practice, but a hostile or corrupted element of some other GType is
 * skipped rather than trusted, the same "never crash, never guess"
 * discipline the rest of this project applies to untrusted config
 * input.
 *
 * The extracted strings are joined with ':' and handed to
 * lc_autoload_normalize() — the exact same pure function the SCALAR
 * path below uses — so trimming and empty-element dropping (D1's
 * threat matrix: "::", a whitespace-only element) is implemented once,
 * not duplicated here.
 */
static gchar **
xfconf_array_to_modules (GPtrArray *array)
{
  GString *joined = g_string_new (NULL);
  gchar **modules;
  guint i;

  for (i = 0; i < array->len; i++)
    {
      GValue *element = g_ptr_array_index (array, i);

      if (!G_VALUE_HOLDS_STRING (element))
        continue;

      if (joined->len > 0)
        g_string_append_c (joined, ':');
      g_string_append (joined, g_value_get_string (element));
    }

  modules = lc_autoload_normalize (joined->str);
  g_string_free (joined, TRUE);

  return modules;
}

gboolean
lc_autoload_xfconf_read (gpointer user_data, LcAutoloadShape *out_shape, gchar ***out_modules)
{
  XfconfChannel *channel = (XfconfChannel *) user_data;
  GValue value = G_VALUE_INIT;

  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (out_shape != NULL, FALSE);
  g_return_val_if_fail (out_modules != NULL, FALSE);

  if (!xfconf_channel_has_property (channel, LC_AUTOLOAD_XFCONF_KEY))
    {
      *out_shape = LC_AUTOLOAD_SHAPE_ABSENT;
      return TRUE;
    }

  /* The key exists but reading it can still fail (e.g. a daemon
   * round-trip error) — that is a genuine LC_AUTOLOAD_READ_FAILED, not
   * an ABSENT key, so it must not be conflated with the has_property()
   * check above. */
  if (!xfconf_channel_get_property (channel, LC_AUTOLOAD_XFCONF_KEY, &value))
    return FALSE;

  if (G_VALUE_HOLDS_STRING (&value))
    {
      *out_shape = LC_AUTOLOAD_SHAPE_SCALAR;
      *out_modules = lc_autoload_normalize (g_value_get_string (&value));
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_PTR_ARRAY))
    {
      *out_shape = LC_AUTOLOAD_SHAPE_ARRAY;
      *out_modules = xfconf_array_to_modules ((GPtrArray *) g_value_get_boxed (&value));
    }
  else
    {
      /* D1's "present, any other type" row: report UNKNOWN and hand
       * back no list at all. lc_autoload_set_enabled() refuses the
       * write outright for this shape — *out_modules is intentionally
       * left untouched here, matching every other caller in this file
       * that only sets it for SCALAR/ARRAY. */
      *out_shape = LC_AUTOLOAD_SHAPE_UNKNOWN;
    }

  g_value_unset (&value);

  return TRUE;
}

/* ---- write helpers ------------------------------------------------------- */

/* The mirror of xfconf_array_to_modules(): builds a GPtrArray of
 * G_TYPE_STRING GValue* from `modules`, in order, for
 * xfconf_channel_set_arrayv(). xfconf_array_free() (declared in
 * <xfconf/xfconf.h>) is the matching destructor — it unsets and frees
 * every element GValue as well as the array itself. */
static GPtrArray *
modules_to_xfconf_array (const gchar *const *modules)
{
  GPtrArray *array = g_ptr_array_new ();

  if (modules != NULL)
    {
      guint i;

      for (i = 0; modules[i] != NULL; i++)
        {
          GValue *element = g_new0 (GValue, 1);

          g_value_init (element, G_TYPE_STRING);
          g_value_set_string (element, modules[i]);
          g_ptr_array_add (array, element);
        }
    }

  return array;
}

gboolean
lc_autoload_xfconf_write (gpointer user_data, LcAutoloadShape shape, const gchar *const *modules)
{
  XfconfChannel *channel = (XfconfChannel *) user_data;

  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);

  switch (shape)
    {
    case LC_AUTOLOAD_SHAPE_ABSENT:
      /* "Delete the key entirely" (lc-autoload.h's contract for this
       * shape on write). xfconf_channel_reset_property() returns void —
       * there is no failure signal to propagate, so this path always
       * reports success to the orchestration layer. */
      xfconf_channel_reset_property (channel, LC_AUTOLOAD_XFCONF_KEY, FALSE);
      return TRUE;

    case LC_AUTOLOAD_SHAPE_SCALAR:
      {
        gchar *joined = lc_autoload_join (modules);
        gboolean ok = xfconf_channel_set_string (channel, LC_AUTOLOAD_XFCONF_KEY, joined);

        g_free (joined);
        return ok;
      }

    case LC_AUTOLOAD_SHAPE_ARRAY:
      {
        GPtrArray *array = modules_to_xfconf_array (modules);
        gboolean ok = xfconf_channel_set_arrayv (channel, LC_AUTOLOAD_XFCONF_KEY, array);

        xfconf_array_free (array);
        return ok;
      }

    case LC_AUTOLOAD_SHAPE_UNKNOWN:
    default:
      /* lc_autoload_set_enabled() never calls write() with UNKNOWN — it
       * refuses the write before reaching here (D1). Reaching this case
       * would be a caller bug, not a real on-disk state to persist. */
      g_return_val_if_reached (FALSE);
    }
}
