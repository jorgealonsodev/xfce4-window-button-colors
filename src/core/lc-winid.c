/* lc-winid.c — see lc-winid.h.
 *
 * NOTE: this is a deliberate RED stub (strict TDD): every function below
 * returns an intentionally wrong constant so tests/test-winid.c fails for
 * the right reasons before the real implementation lands.
 */
#include "lc-winid.h"

gboolean
lc_winid_is_valid (gulong xid)
{
  (void) xid;
  return FALSE; /* WRONG on purpose: RED */
}

gboolean
lc_winid_parse (const gchar *text, gulong *out)
{
  (void) text;
  (void) out;
  return FALSE; /* WRONG on purpose: RED */
}

gchar *
lc_winid_to_string (gulong xid)
{
  (void) xid;
  return NULL; /* WRONG on purpose: RED */
}

gchar *
lc_winid_css_class (gulong xid)
{
  (void) xid;
  return NULL; /* WRONG on purpose: RED */
}
