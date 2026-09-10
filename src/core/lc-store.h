/* lc-store.h — LcStore: record-oriented load/parse of colors.css, and
 * full-document rendering (design.md D8, D14-D16).
 *
 * Scope of this slice (tasks.md 3.1/3.2): load/parse only, plus
 * lc_store_render() as D16 groundwork. This includes validating every
 * rule in a record, not just the base rule (design.md: "The parser
 * rejects a state rule carrying anything else" than background-color).
 * The write path (lc_store_set/has/save) and reconciliation/pruning
 * (D12/D15) are later slices — see tasks.md 4.3/4.4 and 5.1/5.2.
 * Hardening against a marker whose OWN text is malformed or absent
 * (non-hex/over-long fp, mismatched id=, a rule with no marker at all)
 * is task 5.3/5.4: such content is invisible to this slice's marker
 * regex and so simply contributes no entry, rather than being kept
 * unverified per design.md D15.
 *
 * Links glib only.
 */
#ifndef LC_STORE_H
#define LC_STORE_H

#include <glib.h>
#include "lc-css.h"
#include "lc-document.h"

G_BEGIN_DECLS

typedef struct _LcStore LcStore;

/* Loads and parses `path` (expected to be colors.css) into a newly
 * allocated LcStore. NEVER returns NULL and never crashes or aborts —
 * this holds by construction, not by luck: every diagnostic this
 * function can emit, across every branch below, shares one "already
 * warned" flag, so a single call to lc_store_load() emits AT MOST ONE
 * g_warning() in total, never more, no matter how many distinct issues
 * the file has (design.md D8; launcher-color-persistence "Malformed
 * Configuration Degrades Safely").
 *
 * A missing file, an unreadable file, or non-UTF-8 content degrade to an
 * EMPTY store, with nothing propagated to the caller as an error. The
 * file is read here as input only, into an in-memory LcStore: it is
 * never handed to a CSS provider directly, and the only path to
 * rendered CSS is lc_store_render() below.
 *
 * The schema=2 banner comment is a format-migration marker (design.md
 * "Migration / Rollout"), never a validity gate: its absence contributes
 * to the one-warning budget above but NEVER discards otherwise-parseable
 * entries — a hand-edited or truncated file that lost its banner line
 * must not cost the user their colours (design.md "Configuration
 * Formats"; D15; lifecycle step 6b). Parsing always proceeds regardless
 * of whether the banner is present.
 *
 * The parser is record-oriented (design.md "Configuration Formats"): a
 * record is one comment marker, of the form id=<n> fp=<hex> wrapped as a
 * CSS comment, followed by EVERY rule up to the next marker or EOF — not
 * a fixed count, so a fourth state rule needs no parser change. Only the
 * first rule (the base rule) must be exactly "#launcher-<id>
 * #launcher-arrow" for the marker's own id, and only its background-color
 * is read back into the entry — the hover/active colors are always
 * re-derived at render time via lc_color_derive_state(), never parsed.
 * Every rule after the base must be a state rule targeting the same id
 * (selector "<base>:<state>") whose body carries EXACTLY ONE declaration,
 * and it must be background-color (design.md "State rules carry colour
 * only" — the RF-7 safeguard against border-radius/margin desyncing
 * across the three rules). A record violating any of this — wrong or
 * missing base selector, an unparseable base color, a rule targeting a
 * different id, or a state rule with the wrong declarations — is
 * malformed and is skipped whole, leaving every other record unaffected;
 * only the "state rule has the wrong declarations" case contributes to
 * the one-warning budget above, since it is the one case that signals an
 * actual format regression rather than routine hand-editing. Content
 * with no recognizable records at all (e.g. arbitrary non-CSS text)
 * simply yields an empty store — that outcome needs no special-casing,
 * since it falls out of finding no markers to parse. path == NULL is
 * treated the same as an unreadable file.
 *
 * Caller owns the returned store (lc_store_free()). */
LcStore *lc_store_load (const gchar *path);

/* Number of entries currently held by store. store must be non-NULL. */
guint lc_store_size (const LcStore *store);

/* Renders the full colors.css document for store: the schema=2 banner,
 * then one marker comment and its rule triplet (via lc_css_rule_new(),
 * design.md D17/D19) per entry, in load order. This is the SOLE
 * constructor of LcDocument (design.md D16): nothing else in the tree
 * may produce one, so a CSS provider can never be handed anything but a
 * rendered store.
 *
 * store and style must both be non-NULL. Caller owns the returned
 * document (lc_document_free()). */
LcDocument *lc_store_render (LcStore *store, const LcStyle *style);

/* Frees store and every entry it holds. Safe to call with NULL. */
void lc_store_free (LcStore *store);

G_END_DECLS

#endif /* LC_STORE_H */
