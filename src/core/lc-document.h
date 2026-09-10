/* lc-document.h — opaque rendered colors.css document (design.md D16).
 *
 * LcDocument has exactly one constructor: lc_store_render(), declared in
 * lc-store.h and implemented in lc-store.c, which also holds this type's
 * private definition. No "_new_from_string" or "_from_file" constructor
 * exists anywhere in the tree, and none may ever be added here:
 * LcDocument's entire reason to exist is to make it impossible to hand
 * the CSS provider (src/glue/lc-provider.c, a later slice) anything but
 * a rendered, reconciled store.
 *
 * Links glib only.
 */
#ifndef LC_DOCUMENT_H
#define LC_DOCUMENT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _LcDocument LcDocument;

/* Returns the rendered CSS text, borrowed from doc and valid until
 * lc_document_free() is called. Returns NULL if doc is NULL. */
const gchar *lc_document_css (const LcDocument *doc);

/* Frees doc and its rendered CSS text. Safe to call with NULL. */
void lc_document_free (LcDocument *doc);

G_END_DECLS

#endif /* LC_DOCUMENT_H */
