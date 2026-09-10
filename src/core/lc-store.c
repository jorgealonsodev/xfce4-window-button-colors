/* lc-store.c — see lc-store.h. Also implements lc-document.h: this file
 * holds the private definition of LcDocument, so lc_store_render() stays
 * its only constructor (design.md D16).
 */
#include "lc-store.h"

#include <stdarg.h>
#include <string.h>
#include "lc-color.h"
#include "lc-id.h"

/* One parsed colors.css entry: a launcher id, its fingerprint marker
 * text (opaque here — computing and comparing fingerprints is
 * design.md D14, a later slice), and the base rule's color. The
 * hover/active colors are never stored: they are always re-derived from
 * `color` at render time. */
typedef struct
{
  gint    id;
  gchar  *fp;
  LcColor color;
} LcStoreEntry;

struct _LcStore
{
  GPtrArray *entries; /* element-type LcStoreEntry *, in load order */
};

struct _LcDocument
{
  gchar *css;
};

/* The generated-file banner (design.md "Configuration Formats"). Its
 * presence is this slice's cheap structural check for "does this even
 * look like a schema=2 colors.css document" before attempting the
 * record parse: content that lacks it is treated as malformed. */
static const gchar *LC_STORE_BANNER =
  "/* xfce4-window-button-colors — generated file, do not edit by hand. schema=2 */\n";

static void
lc_store_entry_free (gpointer data)
{
  LcStoreEntry *entry = data;

  if (entry == NULL)
    return;

  g_free (entry->fp);
  g_free (entry);
}

static LcStore *
lc_store_new_empty (void)
{
  LcStore *store = g_new0 (LcStore, 1);

  store->entries = g_ptr_array_new_with_free_func (lc_store_entry_free);

  return store;
}

/* Emits `format` as a single g_warning() the first time it is called for
 * a given `warned` flag, and never again for that flag. Every diagnostic
 * lc_store_load() can emit is routed through this, so "at most one
 * warning per lc_store_load() call" (this module's whole never-abort,
 * never-multi-warn contract, design.md D8) holds by construction rather
 * than by each call site independently remembering to check. */
static void
lc_store_warn_once (gboolean *warned, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

static void
lc_store_warn_once (gboolean *warned, const gchar *format, ...)
{
  va_list args;

  if (*warned)
    return;

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, args);
  va_end (args);

  *warned = TRUE;
}

/* Parses the "background-color: rgba(r, g, b, a)" declaration out of a
 * rule body (the text between one rule's braces) into *out, using the
 * caller-supplied compiled regex. Returns FALSE, leaving *out untouched,
 * if no such declaration is found or any channel is out of range. */
static gboolean
lc_store_parse_background_color (GRegex *regex, const gchar *body, LcColor *out)
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

/* True if `selector` is `base_selector` with a pseudo-class directly
 * appended — "<base_selector>:<state>" with no space, matching D18 (the
 * pseudo-class attaches only to the trailing element, exactly as
 * lc_css_rule_new() writes it). Deliberately does not whitelist which
 * state name follows the colon: the marker is the record's delimiter,
 * not a fixed rule count (design.md "A record is one marker line plus
 * every rule that follows it ... it stays correct if a fourth state is
 * ever added"), so any well-formed "<base>:<anything>" selector is
 * accepted here and validated structurally by its declarations instead. */
static gboolean
lc_store_selector_is_state_rule_for (const gchar *selector, const gchar *base_selector)
{
  gsize base_len = strlen (base_selector);

  return g_str_has_prefix (selector, base_selector) &&
         selector[base_len] == ':' &&
         selector[base_len + 1] != '\0';
}

/* True if `decl` (one trimmed, non-empty "property: value" declaration
 * text, without its trailing semicolon) declares background-color. */
static gboolean
lc_store_declaration_is_background_color (const gchar *decl)
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

/* Validates a state rule's declarations (design.md "State rules carry
 * colour only": "`:hover` and `:active` carry exactly one declaration,
 * `background-color`. The parser rejects a state rule carrying anything
 * else" — the RF-7 safeguard against border-radius/margin desyncing
 * across the three rules). `body` must contain exactly one non-empty
 * declaration, and it must be background-color; its value is never read
 * here — hover/active colors are always re-derived at render time via
 * lc_color_derive_state(), never parsed (design.md "Configuration
 * Formats"). */
static gboolean
lc_store_validate_state_rule_declarations (const gchar *body)
{
  gchar **decls = g_strsplit (body, ";", -1);
  guint declaration_count = 0;
  gboolean all_background_color = TRUE;

  for (guint i = 0; decls[i] != NULL; i++)
    {
      gchar *trimmed = g_strdup (decls[i]);

      g_strstrip (trimmed);
      if (trimmed[0] != '\0')
        {
          declaration_count++;
          if (!lc_store_declaration_is_background_color (trimmed))
            all_background_color = FALSE;
        }
      g_free (trimmed);
    }
  g_strfreev (decls);

  return declaration_count == 1 && all_background_color;
}

/* One "<selector> { <body> }" block found while scanning a record. */
typedef struct
{
  gchar *selector; /* trimmed, owned */
  gchar *body;     /* raw text between the braces, owned */
} LcStoreRuleBlock;

static void
lc_store_rule_block_free (gpointer data)
{
  LcStoreRuleBlock *block = data;

  if (block == NULL)
    return;

  g_free (block->selector);
  g_free (block->body);
  g_free (block);
}

/* Scans `text` — one record's span, from just after its marker up to
 * the next marker or EOF (design.md "A record is one marker line plus
 * every rule that follows it until the next marker line or EOF, not
 * 'the next three rules'") — for every "<selector> { <body> }" block it
 * contains, in order. Blank lines between blocks need no special
 * handling: the text between one block's closing brace and the next
 * block's opening brace becomes that next block's (whitespace-trimmed)
 * selector, so a blank line trims away to nothing, and anything else
 * left over simply fails the caller's selector checks rather than being
 * specially recognized. Never returns NULL; a record with no rule at
 * all yields zero blocks. Caller frees with
 * g_ptr_array_free(blocks, TRUE). */
static GPtrArray *
lc_store_scan_rule_blocks (const gchar *text)
{
  GPtrArray *blocks = g_ptr_array_new_with_free_func (lc_store_rule_block_free);
  const gchar *cursor = text;

  for (;;)
    {
      const gchar *open_brace = strchr (cursor, '{');
      const gchar *close_brace;
      LcStoreRuleBlock *block;

      if (open_brace == NULL)
        break;

      close_brace = strchr (open_brace, '}');
      if (close_brace == NULL)
        break;

      block = g_new0 (LcStoreRuleBlock, 1);
      block->selector = g_strndup (cursor, (gsize) (open_brace - cursor));
      g_strstrip (block->selector);
      block->body = g_strndup (open_brace + 1, (gsize) (close_brace - open_brace - 1));
      g_ptr_array_add (blocks, block);

      cursor = close_brace + 1;
    }

  return blocks;
}

/* Parses and validates one full record (design.md "Configuration
 * Formats"): its first block must be exactly the base rule for `id`
 * with a parseable background-color; every following block must be a
 * state rule targeting the same id whose declarations pass
 * lc_store_validate_state_rule_declarations(). Any violation — a
 * missing/wrong base selector, an unparseable base color, a state rule
 * targeting a different id, or a state rule with the wrong declarations
 * — discards the WHOLE record (design.md: "a record whose rules
 * disagree with their marker is malformed and is skipped whole ... a
 * partially applied entry is worse than none"). Returns FALSE, in which
 * case *out_color is left untouched, on any violation.
 *
 * *out_state_rule_violation is set to TRUE only for the specific
 * "wrong declarations" case above — never for a plain missing/mismatched
 * selector or id, which stays a silent skip exactly as it already was
 * before this validation existed. This lets the caller raise the
 * RF-7-specific diagnostic without also warning on every ordinary
 * id-mismatch skip. */
static gboolean
lc_store_parse_record (GRegex *background_color_regex, const gchar *record_text, gint id,
                        LcColor *out_color, gboolean *out_state_rule_violation)
{
  GPtrArray *blocks = lc_store_scan_rule_blocks (record_text);
  gchar *base_selector = lc_id_selector_new (id);
  gboolean ok = FALSE;

  if (base_selector == NULL || blocks->len == 0)
    goto out;

  {
    LcStoreRuleBlock *base = g_ptr_array_index (blocks, 0);

    if (g_strcmp0 (base->selector, base_selector) != 0)
      goto out;

    if (!lc_store_parse_background_color (background_color_regex, base->body, out_color))
      goto out;
  }

  for (guint i = 1; i < blocks->len; i++)
    {
      LcStoreRuleBlock *state = g_ptr_array_index (blocks, i);

      if (!lc_store_selector_is_state_rule_for (state->selector, base_selector))
        goto out; /* wrong id, or not a "<base>:<state>" shape at all */

      if (!lc_store_validate_state_rule_declarations (state->body))
        {
          *out_state_rule_violation = TRUE;
          goto out;
        }
    }

  ok = TRUE;

out:
  g_free (base_selector);
  g_ptr_array_free (blocks, TRUE);

  return ok;
}

/* One matched marker's id, fingerprint text, and byte span within the
 * file, used only while lc_store_parse_records() is deriving record
 * spans. */
typedef struct
{
  gint   id; /* -1 if the marker's id text failed to parse (too large,
              * etc.) — the record is skipped, matching an unparseable
              * id anywhere else in this parser. */
  gchar *fp;
  gint   match_start;
  gint   match_end;
} LcStoreMarkerMatch;

static void
lc_store_marker_match_free (gpointer data)
{
  LcStoreMarkerMatch *match = data;

  if (match == NULL)
    return;

  g_free (match->fp);
  g_free (match);
}

/* Compiles the two fixed regex patterns this parser needs. Both are
 * literal constants that cannot fail to compile in practice, but
 * lc-store.h promises lc_store_load() never crashes or aborts (RNF-6):
 * a compile failure is handled the same way as every other load
 * failure — at most one warning, empty store — rather than asserted, so
 * the "never aborts" contract holds by construction rather than by
 * luck. */
static gboolean
lc_store_compile_regexes (GRegex **out_marker, GRegex **out_background_color, gboolean *warned)
{
  GError *error = NULL;

  *out_marker = g_regex_new ("/\\* id=([0-9]+) fp=([0-9a-fA-F]{1,16}) \\*/", 0, 0, &error);
  if (*out_marker == NULL)
    {
      lc_store_warn_once (warned, "lc_store_load: internal marker pattern failed to compile: %s",
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
      lc_store_warn_once (warned, "lc_store_load: internal color pattern failed to compile: %s",
                           error->message);
      g_error_free (error);
      g_regex_unref (*out_marker);
      *out_marker = NULL;
      return FALSE;
    }

  return TRUE;
}

/* Scans `contents` for every marker and its record span — from just
 * after that marker to the start of the next marker, or EOF for the
 * last one (design.md: "A record is one marker line plus every rule
 * that follows it until the next marker line or EOF"). A record whose
 * rules fully validate becomes one entry; any other record is skipped
 * whole, leaving every other record unaffected.
 *
 * At most one warning is routed through `warned` (lc_store_warn_once())
 * for this whole call when at least one record was dropped specifically
 * because a state rule violated the "background-color only" placement
 * rule (design.md "State rules carry colour only" / RF-7) — a plain
 * missing/mismatched base or id stays a silent skip, unchanged from
 * before this validation existed.
 *
 * TODO(tasks.md 5.4): a rule with no preceding marker at all, or a
 * marker whose OWN text is malformed (non-hex/over-long fp, mismatched
 * "id=") rather than simply followed by a bad record, is not recognized
 * here — such content is invisible to the marker regex and so
 * contributes no entry, kept-but-unverifiable per design.md D15 is a
 * later slice's job, not this one's. */
static void
lc_store_parse_records (LcStore *store, const gchar *contents, gboolean *warned)
{
  GRegex *marker_regex;
  GRegex *background_color_regex;
  GMatchInfo *match_info = NULL;
  GPtrArray *markers;
  gboolean state_rule_violation = FALSE;

  if (!lc_store_compile_regexes (&marker_regex, &background_color_regex, warned))
    return;

  markers = g_ptr_array_new_with_free_func (lc_store_marker_match_free);

  g_regex_match (marker_regex, contents, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      LcStoreMarkerMatch *match = g_new0 (LcStoreMarkerMatch, 1);
      gchar *id_text = g_match_info_fetch (match_info, 1);
      gchar *fp_text = g_match_info_fetch (match_info, 2);

      g_match_info_fetch_pos (match_info, 0, &match->match_start, &match->match_end);
      if (!lc_id_parse (id_text, &match->id))
        match->id = -1;
      match->fp = g_strdup (fp_text);

      g_free (id_text);
      g_free (fp_text);
      g_ptr_array_add (markers, match);

      g_match_info_next (match_info, NULL);
    }
  g_match_info_free (match_info);

  for (guint i = 0; i < markers->len; i++)
    {
      LcStoreMarkerMatch *match = g_ptr_array_index (markers, i);
      gint record_start = match->match_end;
      gint record_end = (i + 1 < markers->len)
                           ? ((LcStoreMarkerMatch *) g_ptr_array_index (markers, i + 1))->match_start
                           : (gint) strlen (contents);
      gchar *record_text;
      LcColor color;
      gboolean this_record_violated_state_rule = FALSE;

      if (match->id < 0)
        continue;

      record_text = g_strndup (contents + record_start, (gsize) (record_end - record_start));

      if (lc_store_parse_record (background_color_regex, record_text, match->id, &color,
                                  &this_record_violated_state_rule))
        {
          LcStoreEntry *entry = g_new0 (LcStoreEntry, 1);

          entry->id = match->id;
          entry->fp = g_strdup (match->fp);
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
    lc_store_warn_once (warned,
                         "lc_store_load: one or more entries were skipped because a state rule "
                         "carried more than its background-color declaration (design.md RF-7)");
}

LcStore *
lc_store_load (const gchar *path)
{
  GError *error = NULL;
  gchar *contents = NULL;
  gsize length = 0;
  LcStore *store;
  gboolean warned = FALSE;

  if (path == NULL)
    {
      lc_store_warn_once (&warned, "lc_store_load: path is NULL; treating as no configuration");
      return lc_store_new_empty ();
    }

  if (!g_file_get_contents (path, &contents, &length, &error))
    {
      lc_store_warn_once (&warned, "lc_store_load: could not read '%s': %s", path, error->message);
      g_error_free (error);
      return lc_store_new_empty ();
    }

  if (length == 0)
    {
      /* An empty file is not malformed input to warn about: it is
       * indistinguishable from a store that has not been written to
       * yet. */
      g_free (contents);
      return lc_store_new_empty ();
    }

  if (!g_utf8_validate (contents, (gssize) length, NULL))
    {
      lc_store_warn_once (&warned, "lc_store_load: '%s' is not valid UTF-8; ignoring", path);
      g_free (contents);
      return lc_store_new_empty ();
    }

  /* The schema=2 banner is a format-migration marker (design.md
   * "Migration / Rollout"), never a validity gate: a hand-edited or
   * truncated file that lost its banner line must not cost the user
   * their colours (design.md "Configuration Formats"; D15; lifecycle
   * step 6b — "never to 'no colours': loading nothing would punish the
   * user for our failure"). So its absence is at most one diagnostic,
   * never a reason to skip parsing or discard whatever IS recoverable. */
  if (strstr (contents, "schema=2") == NULL)
    lc_store_warn_once (&warned,
                         "lc_store_load: '%s' does not carry the schema=2 banner; parsing entries "
                         "anyway",
                         path);

  store = lc_store_new_empty ();
  lc_store_parse_records (store, contents, &warned);

  g_free (contents);

  return store;
}

guint
lc_store_size (const LcStore *store)
{
  g_return_val_if_fail (store != NULL, 0);

  return store->entries->len;
}

LcDocument *
lc_store_render (LcStore *store, const LcStyle *style)
{
  GString *out;
  LcDocument *doc;

  g_return_val_if_fail (store != NULL, NULL);
  g_return_val_if_fail (style != NULL, NULL);

  out = g_string_new (LC_STORE_BANNER);
  g_string_append_c (out, '\n');

  for (guint i = 0; i < store->entries->len; i++)
    {
      LcStoreEntry *entry = g_ptr_array_index (store->entries, i);
      gchar *selector = lc_id_selector_new (entry->id);
      gchar *rule = lc_css_rule_new (selector, &entry->color, style);

      g_string_append_printf (out, "/* id=%d fp=%s */\n", entry->id, entry->fp);
      g_string_append (out, rule);
      g_string_append_c (out, '\n');

      g_free (selector);
      g_free (rule);
    }

  doc = g_new0 (LcDocument, 1);
  doc->css = g_string_free (out, FALSE);

  return doc;
}

void
lc_store_free (LcStore *store)
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
