/* tests/test-winid.c — lc_winid_is_valid / lc_winid_parse /
 * lc_winid_to_string / lc_winid_css_class.
 *
 * The window path keys colours on the X11 window id, so a parse that
 * silently accepts junk would bind a colour to the wrong window, and one
 * that rejects a valid id would lose the user's choice. Both directions
 * are asserted here.
 */
#include <glib.h>
#include <string.h>
#include "lc-winid.h"

/* A representative real XID, taken from the live-panel spike. */
#define SAMPLE_XID 138412036UL

static void
test_winid_zero_is_never_valid (void)
{
  /* 0 is X11's "None" sentinel: it is not a window and must never be
   * treated as one. */
  g_assert_false (lc_winid_is_valid (0));
}

static void
test_winid_ordinary_ids_are_valid (void)
{
  g_assert_true (lc_winid_is_valid (1));
  g_assert_true (lc_winid_is_valid (SAMPLE_XID));
  g_assert_true (lc_winid_is_valid (G_MAXULONG));
}

static void
test_winid_parse_accepts_plain_decimal (void)
{
  gulong out = 0;

  g_assert_true (lc_winid_parse ("138412036", &out));
  g_assert_cmpuint (out, ==, SAMPLE_XID);

  out = 0;
  g_assert_true (lc_winid_parse ("1", &out));
  g_assert_cmpuint (out, ==, 1);
}

static void
test_winid_parse_rejects_malformed_input (void)
{
  const gchar *bad[] = {
    NULL,            /* no input at all */
    "",              /* empty */
    "0",             /* syntactically fine, but None is not a window */
    "+12",           /* signs are not part of the canonical form */
    "-12",
    "12abc",         /* trailing junk */
    "abc",
    " 12",           /* leading whitespace */
    "12 ",           /* trailing whitespace */
    "1 2",           /* embedded whitespace */
    "0x8400050",     /* hex is not the canonical form */
    "1.5",
    "99999999999999999999999999",   /* overflows gulong */
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (bad); i++)
    {
      /* The sentinel proves *out is left untouched on every rejection, so
       * a caller that ignores the return value cannot silently pick up a
       * half-parsed id. */
      gulong out = 0xDEADBEEF;

      g_assert_false (lc_winid_parse (bad[i], &out));
      g_assert_cmpuint (out, ==, 0xDEADBEEF);
    }
}

static void
test_winid_to_string_round_trips (void)
{
  gchar *text = lc_winid_to_string (SAMPLE_XID);
  gulong back = 0;

  g_assert_nonnull (text);
  g_assert_cmpstr (text, ==, "138412036");
  g_assert_true (lc_winid_parse (text, &back));
  g_assert_cmpuint (back, ==, SAMPLE_XID);

  g_free (text);
}

static void
test_winid_to_string_rejects_invalid (void)
{
  g_assert_null (lc_winid_to_string (0));
}

static void
test_winid_css_class_shape (void)
{
  gchar *cls = lc_winid_css_class (SAMPLE_XID);

  g_assert_nonnull (cls);
  g_assert_cmpstr (cls, ==, "lc-win-138412036");

  /* The class is concatenated straight into a CSS selector, so anything
   * that could terminate a selector or open a block would let a malformed
   * id corrupt the whole document. A decimal id cannot, and this asserts
   * the property rather than trusting it. */
  g_assert_null (strpbrk (cls, " \t\n{};,()#.*>+~\"'\\/"));

  g_free (cls);
}

static void
test_winid_css_class_rejects_invalid (void)
{
  g_assert_null (lc_winid_css_class (0));
}

static void
test_winid_css_class_is_unique_per_id (void)
{
  /* Two different windows must never collide onto one class, or colouring
   * one would paint the other. */
  gchar *a = lc_winid_css_class (SAMPLE_XID);
  gchar *b = lc_winid_css_class (SAMPLE_XID + 1);

  g_assert_nonnull (a);
  g_assert_nonnull (b);
  g_assert_cmpstr (a, !=, b);

  g_free (a);
  g_free (b);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/winid/zero-is-never-valid", test_winid_zero_is_never_valid);
  g_test_add_func ("/winid/ordinary-ids-are-valid", test_winid_ordinary_ids_are_valid);
  g_test_add_func ("/winid/parse-accepts-plain-decimal", test_winid_parse_accepts_plain_decimal);
  g_test_add_func ("/winid/parse-rejects-malformed-input", test_winid_parse_rejects_malformed_input);
  g_test_add_func ("/winid/to-string-round-trips", test_winid_to_string_round_trips);
  g_test_add_func ("/winid/to-string-rejects-invalid", test_winid_to_string_rejects_invalid);
  g_test_add_func ("/winid/css-class-shape", test_winid_css_class_shape);
  g_test_add_func ("/winid/css-class-rejects-invalid", test_winid_css_class_rejects_invalid);
  g_test_add_func ("/winid/css-class-is-unique-per-id", test_winid_css_class_is_unique_per_id);

  return g_test_run ();
}
