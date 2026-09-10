/* tests/test-store-load.c — tests for LcStore load/parse and
 * lc_store_render() (tasks.md 3.1/3.2).
 *
 * Covers launcher-color-persistence spec requirement "Malformed
 * Configuration Degrades Safely": a missing, unreadable, or structurally
 * malformed colors.css MUST degrade to an empty store, with at most one
 * warning and no crash (design.md D8). Also covers the record-oriented
 * marker parser's basic contract for a well-formed schema=2 file: a
 * marker immediately followed by its rule triplet is parsed into an
 * entry, and lc_store_render() (design.md D16, the sole constructor of
 * LcDocument) reproduces the marker and reuses lc_css_rule_new() for the
 * rule triplet.
 *
 * Reconciliation/pruning (design.md D12/D15), the write path
 * (lc_store_set/has/save), and hardening the parser against the full
 * threat-matrix fixture set (absent marker, malformed marker, marker
 * with no rule, rule with no marker) are later slices — tasks.md
 * 4.3/4.4 and 5.1-5.4 — and are out of scope here.
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include "lc-store.h"
#include "lc-document.h"
#include "lc-css.h"
#include "lc-color.h"
#include "lc-id.h"

/* Builds one on-disk record (marker + rule triplet) for the given id, fp
 * and hex color, reusing lc_id_selector_new() and lc_css_rule_new() so
 * the fixture never drifts from what the real generator would write.
 * Caller owns the returned string (g_free). */
static gchar *
build_record (gint id, const gchar *fp, const gchar *hex_color, const LcStyle *style)
{
  LcColor color;
  gchar *selector;
  gchar *rule;
  gchar *record;

  g_assert_true (lc_color_parse (hex_color, &color));
  selector = lc_id_selector_new (id);
  g_assert_nonnull (selector);
  rule = lc_css_rule_new (selector, &color, style);
  g_assert_nonnull (rule);

  record = g_strdup_printf ("/* id=%d fp=%s */\n%s", id, fp, rule);

  g_free (selector);
  g_free (rule);

  return record;
}

/* Returns a path to a freshly created, unique temporary file containing
 * `content`. Caller owns the returned path (g_free) and is responsible
 * for removing the file. */
static gchar *
write_temp_css (const gchar *content)
{
  GError *error = NULL;
  gchar *path = NULL;
  gint fd;

  fd = g_file_open_tmp ("lc-store-load-test-XXXXXX.css", &path, &error);
  g_assert_no_error (error);
  g_assert_cmpint (fd, >=, 0);
  g_close (fd, NULL);

  g_assert_true (g_file_set_contents (path, content, -1, &error));
  g_assert_no_error (error);

  return path;
}

static void
test_store_load_missing_file_degrades_to_empty_store_with_one_warning (void)
{
  GError *error = NULL;
  gchar *path = NULL;
  LcStore *store;

  /* g_file_open_tmp() both creates the file and reserves a unique name;
   * remove it immediately so the path is guaranteed not to exist. */
  gint fd = g_file_open_tmp ("lc-store-load-test-missing-XXXXXX.css", &path, &error);
  g_assert_no_error (error);
  g_close (fd, NULL);
  g_assert_cmpint (g_remove (path), ==, 0);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_store_size (store), ==, 0);

  lc_store_free (store);
  g_free (path);
}

static void
test_store_load_unreadable_file_degrades_to_empty_store_with_one_warning (void)
{
  gchar *path = write_temp_css ("/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n");
  LcStore *store;

  g_assert_cmpint (g_chmod (path, 0000), ==, 0);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_store_size (store), ==, 0);

  lc_store_free (store);
  g_chmod (path, 0644);
  g_remove (path);
  g_free (path);
}

static void
test_store_load_malformed_content_degrades_to_empty_store_with_one_warning (void)
{
  /* launcher-color-persistence "Malformed file yields no colors, no
   * crash": arbitrary non-CSS text with NO parseable records at all —
   * not merely "well-formed records with the banner stripped", which is
   * a different, non-malformed case covered separately by
   * entries-survive-a-missing-banner above (that one must keep its
   * entries; this one has none to keep). This fixture also happens to
   * lack the schema=2 banner, which is what supplies this test's one
   * expected warning; the store ends up empty regardless because the
   * content contains no id=/fp= markers to parse, not because the
   * banner is absent. */
  gchar *path = write_temp_css (
    "this is not a colors.css file at all\n"
    "{ unterminated rule ;;; @import \"nope\";\n");
  LcStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_store_size (store), ==, 0);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
}

static void
test_store_load_missing_file_renders_no_markers_or_rules (void)
{
  GError *error = NULL;
  gchar *path = NULL;
  LcStore *store;
  LcDocument *doc;
  const gchar *css;
  LcStyle style = { .corner_radius = 6, .margin = 2 };

  gint fd = g_file_open_tmp ("lc-store-load-test-missing2-XXXXXX.css", &path, &error);
  g_assert_no_error (error);
  g_close (fd, NULL);
  g_assert_cmpint (g_remove (path), ==, 0);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  doc = lc_store_render (store, &style);
  g_assert_nonnull (doc);

  css = lc_document_css (doc);
  g_assert_nonnull (css);
  g_assert_null (strstr (css, "/* id="));
  g_assert_null (strstr (css, "#launcher-"));

  lc_document_free (doc);
  lc_store_free (store);
  g_free (path);
}

static void
test_store_load_well_formed_file_parses_entries (void)
{
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *record_a = build_record (12, "a3f1c8", "#e91e8cd9", &style);
  gchar *record_b = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "%s\n"
    "%s\n",
    record_a, record_b);
  gchar *path = write_temp_css (content);
  LcStore *store;

  store = lc_store_load (path);
  g_assert_nonnull (store);
  g_assert_cmpuint (lc_store_size (store), ==, 2);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (record_a);
  g_free (record_b);
}

static void
test_store_load_entries_survive_a_missing_banner (void)
{
  /* Regression guard (design.md:289: "a truncated or hand-edited file
   * must not cost the user their colours"; D15: "silently wiping a
   * user's colours is the next-worst outcome"; lifecycle step 6b: "never
   * to 'no colours': loading nothing would punish the user for our
   * failure"). A user hand-editing colors.css can lose or mangle the
   * banner comment on the first line without touching anything else. The
   * banner is a format-migration marker (design.md:414), never a
   * validity gate: well-formed entries MUST still be parsed when it is
   * missing, with at most one diagnostic warning and never an emptied
   * store. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *record_a = build_record (12, "a3f1c8", "#e91e8cd9", &style);
  gchar *record_b = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    /* No banner line here at all, unlike every other fixture in this
     * file. */
    "%s\n"
    "%s\n",
    record_a, record_b);
  gchar *path = write_temp_css (content);
  LcStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_store_size (store), ==, 2);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (record_a);
  g_free (record_b);
}

static void
test_store_load_render_reproduces_marker_and_rule_triplet (void)
{
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *record = build_record (12, "a3f1c8", "#e91e8cd9", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "%s\n",
    record);
  gchar *path = write_temp_css (content);
  LcStore *store;
  LcDocument *doc;
  const gchar *css;
  LcColor base_color;
  LcColor hover_color;
  LcColor active_color;
  gchar *css_hover;
  gchar *css_active;

  store = lc_store_load (path);
  g_assert_cmpuint (lc_store_size (store), ==, 1);

  doc = lc_store_render (store, &style);
  g_assert_nonnull (doc);
  css = lc_document_css (doc);
  g_assert_nonnull (css);

  g_assert_nonnull (strstr (css, "schema=2"));
  g_assert_nonnull (strstr (css, "/* id=12 fp=a3f1c8 */"));
  g_assert_nonnull (strstr (css, "#launcher-12 #launcher-arrow {"));
  g_assert_nonnull (strstr (css, "#launcher-12 #launcher-arrow:hover {"));
  g_assert_nonnull (strstr (css, "#launcher-12 #launcher-arrow:active {"));
  g_assert_nonnull (strstr (css, "background-color: rgba(233,30,140,0.85)"));

  /* Hover/active colors are re-derived at render time, never parsed back
   * from the file (design.md "Configuration Formats"). */
  g_assert_true (lc_color_parse ("#e91e8cd9", &base_color));
  hover_color = lc_color_derive_state (&base_color, LC_STATE_HOVER);
  active_color = lc_color_derive_state (&base_color, LC_STATE_ACTIVE);
  css_hover = lc_color_to_css (&hover_color);
  css_active = lc_color_to_css (&active_color);

  g_assert_nonnull (strstr (css, css_hover));
  g_assert_nonnull (strstr (css, css_active));

  g_free (css_hover);
  g_free (css_active);
  lc_document_free (doc);
  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (record);
}

static void
test_store_load_record_with_mismatched_id_is_skipped_whole (void)
{
  /* design.md "Configuration Formats": "Every rule in the record must
   * carry the same ancestor id as the marker. A record whose rules
   * disagree with their marker is malformed and is skipped whole." Here
   * the marker claims id=12 but the rule underneath targets id=99; the
   * well-formed second entry must remain unaffected. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *bad_rule;
  gchar *good_record = build_record (7, "1a2b3c", "#3355ff80", &style);
  LcColor bad_color;
  gchar *bad_selector;
  gchar *content;
  gchar *path;
  LcStore *store;

  g_assert_true (lc_color_parse ("#e91e8cd9", &bad_color));
  bad_selector = lc_id_selector_new (99);
  bad_rule = lc_css_rule_new (bad_selector, &bad_color, &style);

  content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "/* id=12 fp=a3f1c8 */\n"
    "%s\n"
    "%s\n",
    bad_rule, good_record);
  path = write_temp_css (content);

  store = lc_store_load (path);
  g_assert_cmpuint (lc_store_size (store), ==, 1);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (bad_selector);
  g_free (bad_rule);
  g_free (good_record);
}

/* Builds one base rule block's raw text by hand (not via lc_css_rule_new(),
 * which always emits a complete, well-formed triplet): callers need this
 * to pair a valid base rule with a deliberately broken state rule for the
 * malformed-record fixtures below. Reuses lc_id_selector_new()/
 * lc_color_to_css() so the shape still matches the real generator.
 * Caller owns the returned string (g_free). */
static gchar *
build_base_rule_text (gint id, const gchar *hex_color, gint corner_radius, gint margin)
{
  LcColor color;
  gchar *selector;
  gchar *css_color;
  gchar *rule;

  g_assert_true (lc_color_parse (hex_color, &color));
  selector = lc_id_selector_new (id);
  css_color = lc_color_to_css (&color);

  rule = g_strdup_printf (
    "%s {\n"
    "  background-image: none;\n"
    "  background-color: %s;\n"
    "  border-radius: %dpx;\n"
    "  margin: %dpx;\n"
    "}\n",
    selector, css_color, corner_radius, margin);

  g_free (selector);
  g_free (css_color);

  return rule;
}

static void
test_store_load_state_rule_with_extra_declaration_is_skipped_whole (void)
{
  /* design.md "State rules carry colour only": "`:hover` and `:active`
   * carry exactly one declaration, `background-color`. The parser
   * rejects a state rule carrying anything else, so a generator that
   * starts duplicating `border-radius` fails its own round-trip test
   * rather than silently writing three values that can desync under
   * RF-7." A `:hover` rule that duplicates `border-radius` onto itself
   * is exactly that case. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *base_rule = build_base_rule_text (20, "#0a141ea0", 6, 2);
  gchar *bad_hover =
    "#launcher-20 #launcher-arrow:hover {\n"
    "  background-color: rgba(20,30,40,0.60);\n"
    "  border-radius: 6px;\n"
    "}\n";
  gchar *good_record = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "/* id=20 fp=deadbe */\n"
    "%s"
    "%s"
    "\n"
    "%s\n",
    base_rule, bad_hover, good_record);
  gchar *path = write_temp_css (content);
  LcStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_cmpuint (lc_store_size (store), ==, 1);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (base_rule);
  g_free (good_record);
}

static void
test_store_load_state_rule_with_background_image_is_skipped_whole (void)
{
  /* Same RF-7 safeguard as above, exercised via the other kind of
   * violation: the state rule's one declaration is not background-color
   * at all. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *base_rule = build_base_rule_text (21, "#0a141ea0", 6, 2);
  gchar *bad_hover =
    "#launcher-21 #launcher-arrow:hover {\n"
    "  background-image: none;\n"
    "}\n";
  gchar *good_record = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "/* id=21 fp=deadbe */\n"
    "%s"
    "%s"
    "\n"
    "%s\n",
    base_rule, bad_hover, good_record);
  gchar *path = write_temp_css (content);
  LcStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_store_load*");
  store = lc_store_load (path);
  g_test_assert_expected_messages ();

  g_assert_cmpuint (lc_store_size (store), ==, 1);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (base_rule);
  g_free (good_record);
}

static void
test_store_load_fourth_state_rule_parses_fine (void)
{
  /* design.md "A record is one marker line plus every rule that follows
   * it until the next marker line or EOF ... it stays correct if a
   * fourth state is ever added: the marker is the delimiter, the rule
   * count is not." A well-formed record carrying a fourth,
   * currently-hypothetical state rule (":focus") with only
   * background-color must parse without needing any parser change. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *record = build_record (22, "cafe01", "#e91e8cd9", &style);
  gchar *focus_rule =
    "#launcher-22 #launcher-arrow:focus {\n"
    "  background-color: rgba(10,20,30,0.40);\n"
    "}\n";
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "%s"
    "%s\n",
    record, focus_rule);
  gchar *path = write_temp_css (content);
  LcStore *store;

  store = lc_store_load (path);
  g_assert_cmpuint (lc_store_size (store), ==, 1);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (record);
}

static void
test_store_load_state_rule_with_mismatched_id_is_skipped_whole (void)
{
  /* design.md "Every rule in the record must carry the same ancestor id
   * as the marker": here the base rule is correct for id 23, but its
   * `:hover` rule targets id 99. This is the silent-skip path (no
   * warning), unlike the two RF-7-declaration violations above. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *base_rule = build_base_rule_text (23, "#0a141ea0", 6, 2);
  gchar *bad_hover =
    "#launcher-99 #launcher-arrow:hover {\n"
    "  background-color: rgba(1,2,3,0.10);\n"
    "}\n";
  gchar *good_record = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "/* id=23 fp=deadbe */\n"
    "%s"
    "%s"
    "\n"
    "%s\n",
    base_rule, bad_hover, good_record);
  gchar *path = write_temp_css (content);
  LcStore *store;

  store = lc_store_load (path);
  g_assert_cmpuint (lc_store_size (store), ==, 1);

  lc_store_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
  g_free (base_rule);
  g_free (good_record);
}

static void
test_store_load_render_round_trips_byte_identically (void)
{
  /* Now load-bearing for Slice 4b's lc_store_save: render() output must
   * re-parse to an identical store and re-render byte-identically, so
   * writing a store out and reading it back cannot lose or mutate an
   * entry. */
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *record_a = build_record (12, "a3f1c8", "#e91e8cd9", &style);
  gchar *record_b = build_record (7, "1a2b3c", "#3355ff80", &style);
  gchar *content = g_strdup_printf (
    "/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */\n"
    "\n"
    "%s\n"
    "%s\n",
    record_a, record_b);
  gchar *path = write_temp_css (content);
  LcStore *store1;
  LcDocument *doc1;
  const gchar *css1;
  gchar *path2;
  LcStore *store2;
  LcDocument *doc2;
  const gchar *css2;

  store1 = lc_store_load (path);
  g_assert_cmpuint (lc_store_size (store1), ==, 2);
  doc1 = lc_store_render (store1, &style);
  css1 = lc_document_css (doc1);
  g_assert_nonnull (css1);

  path2 = write_temp_css (css1);
  store2 = lc_store_load (path2);
  g_assert_cmpuint (lc_store_size (store2), ==, 2);
  doc2 = lc_store_render (store2, &style);
  css2 = lc_document_css (doc2);
  g_assert_nonnull (css2);

  g_assert_cmpstr (css1, ==, css2);

  lc_document_free (doc1);
  lc_document_free (doc2);
  lc_store_free (store1);
  lc_store_free (store2);
  g_remove (path);
  g_remove (path2);
  g_free (path);
  g_free (path2);
  g_free (content);
  g_free (record_a);
  g_free (record_b);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/store-load/missing-file-degrades-to-empty-store-with-one-warning",
                    test_store_load_missing_file_degrades_to_empty_store_with_one_warning);
  g_test_add_func ("/store-load/unreadable-file-degrades-to-empty-store-with-one-warning",
                    test_store_load_unreadable_file_degrades_to_empty_store_with_one_warning);
  g_test_add_func ("/store-load/malformed-content-degrades-to-empty-store-with-one-warning",
                    test_store_load_malformed_content_degrades_to_empty_store_with_one_warning);
  g_test_add_func ("/store-load/missing-file-renders-no-markers-or-rules",
                    test_store_load_missing_file_renders_no_markers_or_rules);
  g_test_add_func ("/store-load/well-formed-file-parses-entries",
                    test_store_load_well_formed_file_parses_entries);
  g_test_add_func ("/store-load/entries-survive-a-missing-banner",
                    test_store_load_entries_survive_a_missing_banner);
  g_test_add_func ("/store-load/render-reproduces-marker-and-rule-triplet",
                    test_store_load_render_reproduces_marker_and_rule_triplet);
  g_test_add_func ("/store-load/record-with-mismatched-id-is-skipped-whole",
                    test_store_load_record_with_mismatched_id_is_skipped_whole);
  g_test_add_func ("/store-load/state-rule-with-extra-declaration-is-skipped-whole",
                    test_store_load_state_rule_with_extra_declaration_is_skipped_whole);
  g_test_add_func ("/store-load/state-rule-with-background-image-is-skipped-whole",
                    test_store_load_state_rule_with_background_image_is_skipped_whole);
  g_test_add_func ("/store-load/fourth-state-rule-parses-fine",
                    test_store_load_fourth_state_rule_parses_fine);
  g_test_add_func ("/store-load/state-rule-with-mismatched-id-is-skipped-whole",
                    test_store_load_state_rule_with_mismatched_id_is_skipped_whole);
  g_test_add_func ("/store-load/render-round-trips-byte-identically",
                    test_store_load_render_round_trips_byte_identically);

  return g_test_run ();
}
