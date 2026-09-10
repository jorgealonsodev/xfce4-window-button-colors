/* lc-winstore.c — see lc-winstore.h. Also implements lc-document.h: this
 * file holds the private definition of LcDocument, so
 * lc_winstore_render() stays its only constructor.
 */
#include "lc-winstore.h"

#include <glib/gstdio.h>
#include <stdarg.h>
#include <string.h>
#include "lc-css.h"
#include "lc-winid.h"

/* One parsed colors.css entry: a window XID and the base rule's colour.
 * The hover/active colours are never stored: they are always re-derived
 * from `color` at render time via lc_css_rule_new(). */
typedef struct
{
  gulong  xid;
  LcColor color;
} LcWinStoreEntry;

struct _LcWinStore
{
  GPtrArray *entries; /* element-type LcWinStoreEntry *, insertion order */
};

struct _LcDocument
{
  gchar *css;
};

/* The generated-file banner: a format-migration marker, exactly like the
 * launcher-era store's, and for the exact same reason NEVER a validity
 * gate that can discard the user's colours (see lc_winstore_load()). */
static const gchar *LC_WINSTORE_BANNER =
  "/* xfce4-window-button-colors — generated file (window colours), do not edit by hand. schema=1 */\n";

/* Window buttons have no per-window settings surface (unlike the
 * launcher-era corner_radius/margin, which came from settings.ini): the
 * radius/margin below are the fixed visual shape measured and approved
 * on the live panel (spike/tasklist-spike.c's rebuild_css()), reused here
 * as constants rather than threaded through as a parameter nothing yet
 * varies. */
static const LcStyle LC_WINSTORE_STYLE = { .corner_radius = 4, .margin = 0 };

static void
lc_winstore_entry_free (gpointer data)
{
  g_free (data);
}

LcWinStore *
lc_winstore_new (void)
{
  LcWinStore *store = g_new0 (LcWinStore, 1);

  store->entries = g_ptr_array_new_with_free_func (lc_winstore_entry_free);

  return store;
}

static gint
lc_winstore_find_index (const LcWinStore *store, gulong xid)
{
  guint i;

  for (i = 0; i < store->entries->len; i++)
    {
      LcWinStoreEntry *entry = g_ptr_array_index (store->entries, i);

      if (entry->xid == xid)
        return (gint) i;
    }

  return -1;
}

void
lc_winstore_set (LcWinStore *store, gulong xid, const LcColor *color)
{
  gint index;

  if (store == NULL || color == NULL || !lc_winid_is_valid (xid))
    return;

  index = lc_winstore_find_index (store, xid);
  if (index >= 0)
    {
      LcWinStoreEntry *entry = g_ptr_array_index (store->entries, index);

      entry->color = *color;
      return;
    }

  {
    LcWinStoreEntry *entry = g_new0 (LcWinStoreEntry, 1);

    entry->xid = xid;
    entry->color = *color;
    g_ptr_array_add (store->entries, entry);
  }
}

gboolean
lc_winstore_unset (LcWinStore *store, gulong xid)
{
  gint index;

  if (store == NULL)
    return FALSE;

  index = lc_winstore_find_index (store, xid);
  if (index < 0)
    return FALSE;

  g_ptr_array_remove_index (store->entries, (guint) index);

  return TRUE;
}

gboolean
lc_winstore_has (const LcWinStore *store, gulong xid)
{
  if (store == NULL)
    return FALSE;

  return lc_winstore_find_index (store, xid) >= 0;
}

gboolean
lc_winstore_get (const LcWinStore *store, gulong xid, LcColor *out)
{
  gint index;

  if (store == NULL || out == NULL)
    return FALSE;

  index = lc_winstore_find_index (store, xid);
  if (index < 0)
    return FALSE;

  {
    LcWinStoreEntry *entry = g_ptr_array_index (store->entries, index);

    *out = entry->color;
  }

  return TRUE;
}

guint
lc_winstore_size (const LcWinStore *store)
{
  g_return_val_if_fail (store != NULL, 0);

  return store->entries->len;
}

guint
lc_winstore_reconcile (LcWinStore *store, gboolean live_list_obtained,
                        const gulong *live_xids, gsize n_live)
{
  guint dropped = 0;
  guint i;

  g_return_val_if_fail (store != NULL, 0);

  /* Fail-safe direction: an unobtainable live list is never treated as
   * "nothing is open". Keep every entry, prune nothing.
   *
   * An EMPTY-but-successfully-obtained list gets the exact same
   * treatment, and MUST: a snapshot with zero entries is indistinguishable
   * from a snapshot taken before whatever populates it has finished (this
   * was a real, measured production bug — libwnck populates its window
   * list asynchronously; a caller reading it at the wrong moment sees
   * zero windows that are, a fraction of a second later, 29). Choosing
   * the destructive interpretation of that ambiguity once deleted three
   * colours from windows that were still open, silently, with no
   * warning. The asymmetry is deliberate and permanent: keeping a few
   * stale entries costs nothing; wrongly dropping live ones costs the
   * user data they cannot get back. A live_xids/n_live pair therefore
   * only ever authorises a prune when it is non-empty — this function
   * has no way to tell "genuinely zero windows" apart from "not ready
   * yet" and must not guess. */
  if (!live_list_obtained || n_live == 0)
    return 0;

  /* Walk backwards so removing an entry never disturbs the index of the
   * one still to be visited. */
  for (i = store->entries->len; i > 0; i--)
    {
      LcWinStoreEntry *entry = g_ptr_array_index (store->entries, i - 1);
      gboolean still_live = FALSE;
      gsize j;

      for (j = 0; j < n_live; j++)
        {
          if (live_xids[j] == entry->xid)
            {
              still_live = TRUE;
              break;
            }
        }

      if (!still_live)
        {
          g_ptr_array_remove_index (store->entries, i - 1);
          dropped++;
        }
    }

  return dropped;
}

/* Renders store's persisted/CSS text: the banner, then one marker
 * comment and its lc_css_rule_new() rule triplet per entry, in
 * insertion order. Used by BOTH lc_winstore_render() (wrapped into an
 * LcDocument for the CSS provider) and lc_winstore_save() (written
 * verbatim to disk), so the two can never drift out of round-trip sync
 * with each other or with the parser below. Caller owns the returned
 * string (g_free). */
static gchar *
lc_winstore_render_text (const LcWinStore *store)
{
  GString *out = g_string_new (LC_WINSTORE_BANNER);
  guint i;

  g_string_append_c (out, '\n');

  for (i = 0; i < store->entries->len; i++)
    {
      LcWinStoreEntry *entry = g_ptr_array_index (store->entries, i);
      gchar *cls = lc_winid_css_class (entry->xid);
      gchar *selector = g_strdup_printf (".%s", cls);
      gchar *rule = lc_css_rule_new (selector, &entry->color, &LC_WINSTORE_STYLE);

      g_string_append_printf (out, "/* xid=%lu */\n", entry->xid);
      g_string_append (out, rule);
      g_string_append_c (out, '\n');

      g_free (cls);
      g_free (selector);
      g_free (rule);
    }

  return g_string_free (out, FALSE);
}

LcDocument *
lc_winstore_render (const LcWinStore *store)
{
  LcDocument *doc;

  g_return_val_if_fail (store != NULL, NULL);

  doc = g_new0 (LcDocument, 1);
  doc->css = lc_winstore_render_text (store);

  return doc;
}

gboolean
lc_winstore_save (const LcWinStore *store, const gchar *path)
{
  gchar *text;
  gchar *dir;
  GError *error = NULL;
  gboolean ok;

  g_return_val_if_fail (store != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  /* The config directory does not exist yet on a genuinely fresh
   * install (nothing has ever been saved before) — g_file_set_contents()
   * alone cannot create it, so every first save would otherwise fail
   * with ENOENT and no colour would ever persist for a new user.
   * g_mkdir_with_parents() is a silent no-op if the directory (or its
   * parents) already exist. */
  dir = g_path_get_dirname (path);
  g_mkdir_with_parents (dir, 0700);
  g_free (dir);

  text = lc_winstore_render_text (store);
  ok = g_file_set_contents (path, text, -1, &error);
  g_free (text);

  if (!ok)
    {
      g_warning ("lc_winstore_save: could not write '%s': %s", path,
                 error != NULL ? error->message : "(unknown error)");
      g_clear_error (&error);
    }

  return ok;
}

/* ---- load/parse ------------------------------------------------------
 *
 * Carries over lc-store.c's hard-won parser discipline: the banner is a
 * format marker, never a gate; a record whose rules disagree with their
 * own marker is malformed and is skipped WHOLE; at most one warning is
 * emitted per lc_winstore_load() call, no matter how many distinct
 * issues the file has.
 */

static void
lc_winstore_warn_once (gboolean *warned, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

static void
lc_winstore_warn_once (gboolean *warned, const gchar *format, ...)
{
  va_list args;

  if (*warned)
    return;

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, args);
  va_end (args);

  *warned = TRUE;
}

static gboolean
lc_winstore_parse_background_color (GRegex *regex, const gchar *body, LcColor *out)
{
  GMatchInfo *match_info = NULL;
  gboolean found = FALSE;

  if (g_regex_match (regex, body, 0, &match_info))
    {
      gchar *r_text = g_match_info_fetch (match_info, 1);
      gchar *g_text = g_match_info_fetch (match_info, 2);
      gchar *b_text = g_match_info_fetch (match_info, 3);
      gchar *a_text = g_match_info_fetch (match_info, 4);
      gint64 r = g_ascii_strtoll (r_text, NULL, 10);
      gint64 g = g_ascii_strtoll (g_text, NULL, 10);
      gint64 b = g_ascii_strtoll (b_text, NULL, 10);
      gdouble a = g_ascii_strtod (a_text, NULL);

      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 &&
          a >= 0.0 && a <= 1.0)
        {
          out->r = (guint8) r;
          out->g = (guint8) g;
          out->b = (guint8) b;
          out->a = (guint8) CLAMP ((gint) (a * 255.0 + 0.5), 0, 255);
          found = TRUE;
        }

      g_free (r_text);
      g_free (g_text);
      g_free (b_text);
      g_free (a_text);
    }

  g_match_info_free (match_info);

  return found;
}

static gboolean
lc_winstore_selector_is_state_rule_for (const gchar *selector, const gchar *base_selector)
{
  gsize base_len = strlen (base_selector);

  return g_str_has_prefix (selector, base_selector) &&
         selector[base_len] == ':' &&
         selector[base_len + 1] != '\0';
}

static gboolean
lc_winstore_declaration_is_background_color (const gchar *decl)
{
  const gchar *colon = strchr (decl, ':');
  gchar *property;
  gboolean is_background_color;

  if (colon == NULL)
    return FALSE;

  property = g_strndup (decl, (gsize) (colon - decl));
  g_strstrip (property);
  is_background_color = (g_strcmp0 (property, "background-color") == 0);
  g_free (property);

  return is_background_color;
}

static gboolean
lc_winstore_validate_state_rule_declarations (const gchar *body)
{
  gchar **decls = g_strsplit (body, ";", -1);
  guint declaration_count = 0;
  gboolean all_background_color = TRUE;
  guint i;

  for (i = 0; decls[i] != NULL; i++)
    {
      gchar *trimmed = g_strdup (decls[i]);

      g_strstrip (trimmed);
      if (trimmed[0] != '\0')
        {
          declaration_count++;
          if (!lc_winstore_declaration_is_background_color (trimmed))
            all_background_color = FALSE;
        }
      g_free (trimmed);
    }
  g_strfreev (decls);

  return declaration_count == 1 && all_background_color;
}

typedef struct
{
  gchar *selector;
  gchar *body;
} LcWinStoreRuleBlock;

static void
lc_winstore_rule_block_free (gpointer data)
{
  LcWinStoreRuleBlock *block = data;

  if (block == NULL)
    return;

  g_free (block->selector);
  g_free (block->body);
  g_free (block);
}

static GPtrArray *
lc_winstore_scan_rule_blocks (const gchar *text)
{
  GPtrArray *blocks = g_ptr_array_new_with_free_func (lc_winstore_rule_block_free);
  const gchar *cursor = text;

  for (;;)
    {
      const gchar *open_brace = strchr (cursor, '{');
      const gchar *close_brace;
      LcWinStoreRuleBlock *block;

      if (open_brace == NULL)
        break;

      close_brace = strchr (open_brace, '}');
      if (close_brace == NULL)
        break;

      block = g_new0 (LcWinStoreRuleBlock, 1);
      block->selector = g_strndup (cursor, (gsize) (open_brace - cursor));
      g_strstrip (block->selector);
      block->body = g_strndup (open_brace + 1, (gsize) (close_brace - open_brace - 1));
      g_ptr_array_add (blocks, block);

      cursor = close_brace + 1;
    }

  return blocks;
}

static gboolean
lc_winstore_parse_record (GRegex *background_color_regex, const gchar *record_text, gulong xid,
                           LcColor *out_color, gboolean *out_state_rule_violation)
{
  GPtrArray *blocks = lc_winstore_scan_rule_blocks (record_text);
  gchar *cls = lc_winid_css_class (xid);
  gchar *base_selector = cls != NULL ? g_strdup_printf (".%s", cls) : NULL;
  gboolean ok = FALSE;
  guint i;

  if (base_selector == NULL || blocks->len == 0)
    goto out;

  {
    LcWinStoreRuleBlock *base = g_ptr_array_index (blocks, 0);

    if (g_strcmp0 (base->selector, base_selector) != 0)
      goto out;

    if (!lc_winstore_parse_background_color (background_color_regex, base->body, out_color))
      goto out;
  }

  for (i = 1; i < blocks->len; i++)
    {
      LcWinStoreRuleBlock *state = g_ptr_array_index (blocks, i);

      if (!lc_winstore_selector_is_state_rule_for (state->selector, base_selector))
        goto out;

      if (!lc_winstore_validate_state_rule_declarations (state->body))
        {
          *out_state_rule_violation = TRUE;
          goto out;
        }
    }

  ok = TRUE;

out:
  g_free (cls);
  g_free (base_selector);
  g_ptr_array_free (blocks, TRUE);

  return ok;
}

typedef struct
{
  gulong xid;   /* only meaningful when xid_valid is TRUE */
  gboolean xid_valid;
  gint match_start;
  gint match_end;
} LcWinStoreMarkerMatch;

static void
lc_winstore_marker_match_free (gpointer data)
{
  g_free (data);
}

static gboolean
lc_winstore_compile_regexes (GRegex **out_marker, GRegex **out_background_color, gboolean *warned)
{
  GError *error = NULL;

  *out_marker = g_regex_new ("/\\* xid=([0-9]+) \\*/", 0, 0, &error);
  if (*out_marker == NULL)
    {
      lc_winstore_warn_once (warned, "lc_winstore_load: internal marker pattern failed to compile: %s",
                              error->message);
      g_error_free (error);
      return FALSE;
    }

  *out_background_color = g_regex_new (
    "background-color:\\s*rgba\\(\\s*([0-9]{1,3})\\s*,\\s*([0-9]{1,3})\\s*,"
    "\\s*([0-9]{1,3})\\s*,\\s*([0-9]*\\.?[0-9]+)\\s*\\)",
    0, 0, &error);
  if (*out_background_color == NULL)
    {
      lc_winstore_warn_once (warned, "lc_winstore_load: internal color pattern failed to compile: %s",
                              error->message);
      g_error_free (error);
      g_regex_unref (*out_marker);
      *out_marker = NULL;
      return FALSE;
    }

  return TRUE;
}

static void
lc_winstore_parse_records (LcWinStore *store, const gchar *contents, gboolean *warned)
{
  GRegex *marker_regex;
  GRegex *background_color_regex;
  GMatchInfo *match_info = NULL;
  GPtrArray *markers;
  gboolean state_rule_violation = FALSE;
  guint i;

  if (!lc_winstore_compile_regexes (&marker_regex, &background_color_regex, warned))
    return;

  markers = g_ptr_array_new_with_free_func (lc_winstore_marker_match_free);

  g_regex_match (marker_regex, contents, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      LcWinStoreMarkerMatch *match = g_new0 (LcWinStoreMarkerMatch, 1);
      gchar *xid_text = g_match_info_fetch (match_info, 1);

      g_match_info_fetch_pos (match_info, 0, &match->match_start, &match->match_end);
      match->xid_valid = lc_winid_parse (xid_text, &match->xid);

      g_free (xid_text);
      g_ptr_array_add (markers, match);

      g_match_info_next (match_info, NULL);
    }
  g_match_info_free (match_info);

  for (i = 0; i < markers->len; i++)
    {
      LcWinStoreMarkerMatch *match = g_ptr_array_index (markers, i);
      gint record_start = match->match_end;
      gint record_end = (i + 1 < markers->len)
                           ? ((LcWinStoreMarkerMatch *) g_ptr_array_index (markers, i + 1))->match_start
                           : (gint) strlen (contents);
      gchar *record_text;
      LcColor color;
      gboolean this_record_violated_state_rule = FALSE;

      if (!match->xid_valid)
        continue;

      record_text = g_strndup (contents + record_start, (gsize) (record_end - record_start));

      if (lc_winstore_parse_record (background_color_regex, record_text, match->xid, &color,
                                     &this_record_violated_state_rule))
        {
          LcWinStoreEntry *entry = g_new0 (LcWinStoreEntry, 1);

          entry->xid = match->xid;
          entry->color = color;
          g_ptr_array_add (store->entries, entry);
        }
      else if (this_record_violated_state_rule)
        {
          state_rule_violation = TRUE;
        }

      g_free (record_text);
    }

  g_ptr_array_free (markers, TRUE);
  g_regex_unref (marker_regex);
  g_regex_unref (background_color_regex);

  if (state_rule_violation)
    lc_winstore_warn_once (warned,
                            "lc_winstore_load: one or more entries were skipped because a state rule "
                            "carried more than its background-color declaration");
}

LcWinStore *
lc_winstore_load (const gchar *path)
{
  GError *error = NULL;
  gchar *contents = NULL;
  gsize length = 0;
  LcWinStore *store;
  gboolean warned = FALSE;

  if (path == NULL)
    {
      lc_winstore_warn_once (&warned, "lc_winstore_load: path is NULL; treating as no configuration");
      return lc_winstore_new ();
    }

  if (!g_file_get_contents (path, &contents, &length, &error))
    {
      /* A missing file is the ordinary first-run case (nothing has ever
       * been saved yet), not a malformed one: g_debug, not g_warning, so
       * a normal first panel start stays quiet. Anything else — denied
       * permissions, a directory where the file should be, and so on —
       * is still a g_warning. */
      if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_debug ("lc_winstore_load: '%s' does not exist yet; starting with no colours", path);
      else
        lc_winstore_warn_once (&warned, "lc_winstore_load: could not read '%s': %s", path, error->message);
      g_error_free (error);
      return lc_winstore_new ();
    }

  if (length == 0)
    {
      /* An empty file is indistinguishable from a store never written
       * to; not malformed input, no warning. */
      g_free (contents);
      return lc_winstore_new ();
    }

  if (!g_utf8_validate (contents, (gssize) length, NULL))
    {
      lc_winstore_warn_once (&warned, "lc_winstore_load: '%s' is not valid UTF-8; ignoring", path);
      g_free (contents);
      return lc_winstore_new ();
    }

  /* The banner is a format-migration marker, never a validity gate: its
   * absence is at most one diagnostic, never a reason to skip parsing or
   * discard whatever IS recoverable. */
  if (strstr (contents, "schema=1") == NULL)
    lc_winstore_warn_once (&warned,
                            "lc_winstore_load: '%s' does not carry the schema=1 banner; parsing "
                            "entries anyway",
                            path);

  store = lc_winstore_new ();
  lc_winstore_parse_records (store, contents, &warned);

  g_free (contents);

  return store;
}

void
lc_winstore_free (LcWinStore *store)
{
  if (store == NULL)
    return;

  g_ptr_array_free (store->entries, TRUE);
  g_free (store);
}

const gchar *
lc_document_css (const LcDocument *doc)
{
  if (doc == NULL)
    return NULL;

  return doc->css;
}

void
lc_document_free (LcDocument *doc)
{
  if (doc == NULL)
    return;

  g_free (doc->css);
  g_free (doc);
}
