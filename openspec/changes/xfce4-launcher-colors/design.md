# Design: xfce4-launcher-colors

Inputs: `proposal.md` (8-slice plan), `research.md` (authoritative evidence), PRD v1.1 §7.3/§7.4, `openspec/config.yaml` `rules.design`.
No claim below contradicts `research.md`. Two refinements go beyond it and are flagged `[+]`.

## Technical Approach

One in-process GTK3 module inside `xfce4-panel`, split into a **pure core that never links GTK** and a **thin GTK/panel glue layer**. All logic that can be wrong (selectors, ids, colours, file formats, xfconf list edits, the process guard) lives in the core and is unit-tested headless under `meson test`. The glue is deliberately dumb: detect, insert, render, reload.

    /Gtk/Modules ──► GTK3 loads .so in every GTK3 process
                       └► gtk_module_init → lc_guard_should_activate(g_get_prgname())
                            │ false → return (one strcmp, nothing allocated)
                            └ true  → load store ─► reconcile (D12 + D14) ─► attach ONE GtkCssProvider
                                      └ ONE emission hook on GtkWidget::map
                                          └ per launcher: 3 menu items inserted once
    user picks colour ─► lc_store_set ─► lc_store_render ─► load_from_data (instant, RF-3)
                                                        └─► atomic write colors.css (RF-4)

## Architecture Decisions

| # | Decision | Choice | Rejected alternative | Rationale |
|---|---|---|---|---|
| D1 | Layering | `src/core/` (glib+gio only) vs `src/glue/` (GTK/panel/xfconf) | One flat module | Core is the only testable seam; slice 1 then needs only meson+ninja+glib-dev, not the panel headers `[+]` |
| D2 | Process guard | Pure predicate `lc_guard_should_activate(const char *prgname)` called first in `gtk_module_init` | Inline `g_strcmp0` in glue | Makes RNF-6's most important branch unit-testable (`NULL`, `""`, `"xfce4-panel-wrapper"`) |
| D3 | Detection | One `g_signal_add_emission_hook` on `map`, id from `g_signal_lookup("map", GTK_TYPE_WIDGET)` at init | `gtk_window_list_toplevels()` polling | Polling violates RNF-2; hook is O(1) setup, event-driven (research lane 4) |
| D4 | Plugin resolution | Match the plugin GObject itself (it carries the `launcher-<id>` name); ancestor walk only as a compiled-in fallback behind `lc_hook_resolve_plugin()` | Walk ancestors on every match | The inner `launcher-arrow` button also matches the name prefix; a walk per button map is wasted work. Containers map before children, so the plugin's own `map` fires |
| D5 | Selector | `#launcher-<id> #launcher-arrow`, generated only by `lc_id_selector_new(id, state)`; the state pseudo-class is appended to the **last** element by the function itself | `#launcher-<id>` alone; `#launcher-button`; assembling selectors by concatenation at call sites | Inner button paints and hovers; its effective name is `launcher-arrow` (research lane 3, three tags). `#launcher-button` matches nothing and fails silently. GTK3 permits a state pseudo-class only on the last element of a chain, so `#launcher-<id>:hover #launcher-arrow` is the same silent-no-match trap — see D18 |
| D6 | Provider count | One screen/widget provider, whole-document reload | One provider per launcher | RNF-1/RNF-3; a document reload is one `load_from_data` |
| D7 | Provider priority | **Single decision point**: `LC_PROVIDER_PRIORITY` + `lc_provider_attach()` in `src/glue/lc-provider.c`, driven by meson option `-Dprovider_priority=application\|user`. No other file may name a priority constant or an attach function. **Confirmed default: `APPLICATION` (600)** | Priority literal at each call site; `USER` (800) | **Answered empirically by the slice-2 spike** (`docs/spike-findings.md`): `APPLICATION` wins at both screen and widget scope, so `USER` is not needed and the user keeps the last word through their own `gtk.css`. Widget-level outranking screen-level at equal priority was confirmed at the same time. The single decision point stays — it is now what records the answer rather than what defers it |
| D8 | CSS input trust | Never `load_from_file(colors.css)`. Always parse → `LcStore` → `lc_store_render()` → `load_from_data()` | Hand the user file straight to the provider | Bounds a user-writable file to rules our generator can emit; malformed input degrades to "no colours" |
| D9 | Menu timing | Insert all 3 items at detection, before any right-click | Insert on popup | The menu is built once inside `if (menu == NULL)`; later inserts are invisible (research lane 2) |
| D10 | RF-5 visibility | `gtk_widget_set_no_show_all(TRUE)` + `gtk_widget_set_visible()` on the inserted item; refreshed on every store mutation **and** on the plugin's `button-press-event` (button 3) | `xfce_panel_plugin_menu_destroy()` then re-insert | `menu_destroy()` frees **every** module's custom items on that plugin and is a no-op while the menu is visible. It must never be called |
| D11 | Dedup | `g_object_set_data_full(plugin, "lc-attachment", …)` checked first; also the destroy hook | Keyed on `unique_id` | `map` re-fires on panel autohide/show; the flag is per widget instance and dies with it |
| D12 | RF-8 liveness | `lc_store_prune(store, live_ids, n)` is pure; live ids come from the xfconf `xfce4-panel` channel, not from the map registry `[+]` | Prune against widgets seen mapped | A launcher on a hidden/other panel may never map; pruning from the registry would delete a live colour |
| D13 | Autoload | Pure list transforms `lc_autoload_add/remove/is_enabled` over `gchar**`; xfconf I/O behind a 2-function `LcAutoloadBackend` vtable | Call xfconf inline | Lets `--enable` idempotency and "must not drop other modules" be unit-tested with a fake backend |
| D14 | Colour identity | An entry binds to **id + fingerprint** of the launcher's `items` desktop-file list, carried as a `/* id=<n> fp=<hex6> */` marker; recomputed at load, discarded on genuine mismatch | Id alone (D12 only) | **Verified empirically on the live 4.18.4 panel: ids are recycled and the removed plugin's xfconf subtree is not fully cleaned.** `plugin-12` and `plugin-13` are launchers still carrying PulseAudio and clock keys. If the module is unloaded when a launcher is removed (RF-6 off, or a panel restart), a later launcher on that recycled id would silently inherit a colour the user never chose — the exact confusion this product exists to remove |
| D15 | Reconciliation policy | Pure verdict function `lc_store_reconcile_entry()` returning `KEEP` / `DROP_NOT_LAUNCHER` / `DROP_FINGERPRINT`; **an uncomputable fingerprint KEEPS the entry** | Discard on any doubt | RNF-6 forbids crashing the panel, and silently wiping a user's colours is the next-worst outcome. Discarding is reserved for a fingerprint that was computed successfully and genuinely differs. Making the policy a pure truth table puts the whole fail-safe direction under unit test |
| D16 | Startup ordering | **Reconcile in memory, then feed the provider the pruned document via `load_from_data()`. The on-disk file is input only; it is rewritten afterwards and only when reconciliation actually changed something.** Enforced by signature: the provider accepts only an opaque `LcDocument *`, whose sole constructor is `lc_store_render()` | Write the pruned file back first, then point the provider at it; or enforce the order by convention and code review | `GtkCssProvider` knows nothing about the marker — it treats `/* id=… fp=… */` as an inert comment and applies every rule it finds. A provider that loads before reconciliation paints stale and mismatched colours, so the fingerprint buys nothing at startup. The reject-shape also puts a disk write on the critical path of every session start (RNF-3) and rewrites the user's file when nothing changed. This is D8 made load-bearing rather than incidental. Convention was rejected because the failure is **silent wrong output**: nothing errors, no test fails, and the user simply sees a launcher wearing a colour they never chose — the exact symptom D14 exists to prevent. A `const char *` parameter was rejected too: it still compiles when handed `g_file_get_contents("colors.css")`, which is precisely the bypass being closed |
| D17 | Rule template | Every entry emits **three** rules: base with `background-image: none`, `:hover` brighter, `:active` darker. **Geometry (`border-radius`, `margin`) and the image suppression live on the base rule only; state rules carry `background-color` and nothing else** | `background-color` alone; repeating geometry on all three rules | Spike evidence: **Adwaita never painted at any priority or scope** — `baseline-Adwaita.png` and `painted-Adwaita-user.png` are pixel-identical — because Adwaita's `GtkButton` sets an opaque `background-image` that occludes `background-color`. Separately, a flat background overrode the theme's prelight and **hover feedback was lost** (`hover-Mint-Y-Dark-Pink-application.png` identical to the painted frame), so a coloured launcher stopped signalling that it is hoverable. That is a usability regression, not a cosmetic one |
| D18 | State pseudo-class placement | The pseudo-class goes on the **last** element only: `#launcher-<id> #launcher-arrow:hover`. `lc_id_selector_new(id, state)` is the sole producer and appends it by construction, so the inverted form cannot be expressed at a call site | Let call sites concatenate `":hover"` where convenient | GTK3 permits state pseudo-classes only on the final element of a selector chain. `#launcher-<id>:hover #launcher-arrow` matches nothing and fails **silently** — the identical failure shape to the `#launcher-button` trap that D5 exists to prevent. Structural prevention rather than a comment, for the same reason as D16 |
| D19 | State colour derivation | Pure `lc_color_derive_state()` in `src/core/lc-color.c`: blend toward white or black by a fixed fraction, **with the target chosen by luminance so the extremes have headroom**, plus a partial alpha blend toward opaque | Fixed `+15%` lightness; alpha untouched; alpha forced to 1.0 | A fixed lightness bump is a no-op on white and on black, which are exactly the colours a user picks for a monochrome panel. Leaving alpha untouched makes hover invisible on a nearly transparent colour, so the regression D17 fixes would come straight back. Forcing opacity would discard the user's deliberate transparency |

## Module Lifecycle and Failure Paths

`G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);`
`G_MODULE_EXPORT void gtk_module_exit (void);`

| Step | Failure | Behaviour (RNF-6: never abort, never crash) |
|---|---|---|
| 1. `lc_guard_should_activate(g_get_prgname())` | not the panel | `return` immediately — nothing allocated, no hook, no provider. Cost in other GTK3 apps: one string compare |
| 2. static `initialized` flag | double load | `return`; init and exit are idempotent |
| 3. resolve XDG paths | `NULL` home | one `g_warning`, `return` |
| 4. `lc_settings_load(settings.ini)` | any error | built-in defaults (radius 6, margin 2). **Must precede any render**, because the style is baked into every rule |
| 5. `lc_store_load(colors.css)` | missing / unreadable / malformed | empty store, one `g_warning`, continue. Parsed into `LcStore` — the raw file never reaches the provider (D8) |
| 6. reconcile: one `xfconf_channel_get_properties("/plugins")`, then `lc_store_reconcile_entry()` per entry (D12 + D14) | xfconf unreadable, `/plugins` missing, `items` missing or of unexpected type | **every affected entry is KEPT** (`items_readable = FALSE`), one `g_warning`, continue. Our inability to read xfconf is never treated as evidence that a launcher changed |
| 6b. reconcile cannot run at all — no xfconf connection, `/plugins` absent, the whole call fails | — | **the store is used as loaded, every entry kept**, one `g_warning`. Degraded to pre-D14 behaviour, never to "no colours": loading nothing would punish the user for our failure. This path still goes through `lc_store_render()` like every other, so D8's input bound and D16's ordering guarantee both hold unchanged |
| 7. `LcDocument *doc = lc_store_render(...)` → `lc_provider_attach(screen, doc)` → `load_from_data()` internally — **the provider cannot be handed anything but a rendered document** (D16) | CSS parse error captured via `GError` | discard rule set, warn once, continue |
| 8. write-back, **only if step 6 actually dropped an entry** (RF-8) | nothing dropped | **no write at all** — a session that changes nothing must not touch the user's file or its mtime |
| 8b. write-back | write fails | the reconciled colours are already live from step 7; warn once, continue. Persistence is retried at the next mutation |
| 9. `g_signal_lookup("map", GTK_TYPE_WIDGET)` | returns 0 | warn once, keep the provider attached; colours still apply, menu items never appear (degraded, not fatal) |
| 10. `g_signal_add_emission_hook(...)` | — | store hook id for teardown |
| any | — | no `g_error`/`g_assert`/`abort` on shipped paths; every GTK/panel call site null-checks and `GTK_IS_*`/`XFCE_IS_PANEL_PLUGIN`-checks first |

### Startup ordering is a correctness constraint, not a preference (D16)

`GtkCssProvider` has no knowledge of the fingerprint. It parses `colors.css` as ordinary CSS, treats `/* id=12 fp=a3f1c8 */` as an inert comment, and applies every rule it finds. **A provider that loads before reconciliation therefore paints stale and mismatched colours, and the whole D14 mechanism buys nothing at startup** — the wrong colour appears and only clears if something later rewrites the file.

Steps 5 → 6 → 7 are ordered, and step 7 is the only path to the provider:

    colors.css ──read──► LcStore ──reconcile (D12+D14)──► lc_store_render() ──► load_from_data()
       (input only)                      │                                          ▲
                                         └── dropped something? ──► write back ─────┘ (after, off the critical path)

Three consequences that implementation must not lose:

- **The file is input only.** Nothing calls `gtk_css_provider_load_from_file()` on `colors.css`, ever. This is D8 restated as an ordering requirement rather than an injection-bounding one — the same rule now earns its keep twice.
- **A no-op reconciliation writes nothing.** If step 6 drops no entry, `colors.css` is not rewritten and its mtime is untouched. Rewriting an unchanged file on every login is needless disk churn, defeats any future mtime-based tooling, and would make the `GFileMonitor` self-trigger.
- **The write is never on the critical path.** Colours are live after step 7; step 8 only satisfies RF-8's "leave no rubbish". This matches the mutation path, where the visual apply also precedes the disk write.

#### Enforced by signature, not by convention

The ordering above is guaranteed by the type system rather than by review discipline, because the failure mode is silent wrong output: no error, no failing test, just a launcher wearing a colour the user never chose.

```c
/* src/core/lc-document.h — opaque, definition private to lc-store.c */
typedef struct _LcDocument LcDocument;
LcDocument *lc_store_render  (LcStore *s, const LcStyle *st);   /* the ONLY constructor */
const char *lc_document_css  (const LcDocument *d);             /* borrowed, read-only */
void        lc_document_free (LcDocument *d);
```

`LcDocument` has no `lc_document_new_from_string()`, no `_from_file()`, and no other constructor — by design, and this is the invariant a future change must not break. An `LcDocument` can therefore only exist if an `LcStore` was rendered, and an `LcStore` only exists after load and reconciliation. Handing the provider raw file bytes does not compile.

**The complete set of functions that can reach the provider**, all in `src/glue/lc-provider.c`, which holds the one `GtkCssProvider *` as a file-scope `static` never exposed by any header or accessor:

| Function | CSS input | Notes |
|---|---|---|
| `void lc_provider_attach (GdkScreen *screen, const LcDocument *doc)` | `LcDocument *` only | Called once, at step 7. Also the sole owner of `LC_PROVIDER_PRIORITY` and attach scope (D7) |
| `void lc_provider_set_document (const LcDocument *doc)` | `LcDocument *` only | Every later reload — colour mutation, RF-7 style change, `GFileMonitor` |
| `void lc_provider_detach (void)` | none | `gtk_module_exit` |

No fourth function exists, and none returns the provider, so `gtk_css_provider_load_from_*` appears exactly once in the tree. The three call sites that reload — colour mutation, style change, and the `GFileMonitor` on `colors.css` — all re-enter through load → reconcile → `lc_store_render()`. The monitor path is the one worth naming explicitly: it is the only reload triggered by an external file change, so it is the most tempting place to shortcut straight to the file, and it must not.

This is a compile-time barrier, not a lint rule; the grep guard in the test suite is a second line of defence, not the mechanism.

`gtk_module_exit`: remove the emission hook, detach the provider, free the registry — guarded by the same flag, best-effort, because GTK modules are normally never unloaded.

## Detection Hook — Filtering Ladder and Cost

The callback **always returns `TRUE`** (stay installed) and does nothing on anything unexpected.

| Order | Check | Cost for a non-launcher widget |
|---|---|---|
| 1 | `GTK_IS_WIDGET(instance)` | one type check |
| 2 | `gtk_widget_get_name(w)[0] != 'l'` → out | one byte compare — this is the RNF-2 answer |
| 3 | `g_str_has_prefix(name, "launcher-")` | only for `l…` names |
| 4 | `XFCE_IS_PANEL_PLUGIN(w)` | rejects the inner `launcher-arrow` button |
| 5 | `xfce_panel_plugin_get_unique_id()` → `gint`, reject `< 0` | — |
| 6 | `g_object_get_data(plugin, "lc-attachment")` set → out (D11) | — |
| 7 | insert 3 menu items, register attachment with a destroy notify | once per launcher |

Steps 2–4 measured against RNF-2/RNF-3 in H0-b.

## Launcher Identity — Fingerprint (D14)

Plugin ids are recycled and the removed plugin's xfconf subtree survives, so an id is not an identity.

**Input.** The `items` string array at `/plugins/plugin-<id>/items` in the `xfce4-panel` channel — the launcher's desktop-file list. Nothing else contributes: not the position, not the panel, not the label, so RF-9 (colour survives a move) is preserved by construction.

**Normalisation**, in this order:
1. Drop `NULL` and empty elements; trim leading and trailing ASCII whitespace from each.
2. **Sort byte-wise (`strcmp`), not by locale collation.** Locale-dependent ordering would make the fingerprint vary with `LC_COLLATE` between sessions, breaking the stability requirement.
3. Frame each element length-prefixed as `<byte-length>:<bytes>` and concatenate. Rejected: joining on a separator — any separator is theoretically forgeable inside an element, and framing removes the ambiguity for free.
4. Hash the raw UTF-8 bytes as xfconf stores them. No case folding and no Unicode normalisation: the values are filenames the panel itself wrote, and NFC/NFD folding would add a dependency for no gain.

Sorting makes the desktop-file **set** the identity. Reordering the actions inside one launcher therefore keeps its colour, while a recycled id pointing at a genuinely different `.desktop` loses it. Rejected: order-preserving, which would drop the colour on a harmless reorder.

**Hash.** `g_compute_checksum_for_string (G_CHECKSUM_SHA256, framed, -1)`, truncated to the **first 6 lowercase hex characters** (24 bits) — matching the agreed marker shape, and already in GLib so no dependency is added. A collision means one entry is kept that should have been dropped; at ~6·10⁻⁸ per comparison and 22 launchers this is ~10⁻⁶ per load, against a failure that is cosmetic and user-correctable.

**Cost against RNF-3 (< 10 ms).** The load-time pass issues **one** `xfconf_channel_get_properties(channel, "/plugins")` call — a single D-Bus round trip returning the whole subtree as a `GHashTable` — and every per-entry lookup is then in memory. This is the same call D12 already needs for live-id enumeration, so it is shared, not additive. SHA-256 over a few hundred bytes is sub-microsecond; 22 entries cost well under 0.1 ms of hashing. Rejected: one `xfconf_channel_get_arrayv()` per entry — 22 D-Bus round trips, which is the only shape here that could actually threaten RNF-3.

**Layer split held intact.** Computation is pure and takes a plain string vector; reading `items` from xfconf is glue.

```c
/* src/core/lc-fingerprint.h — links glib only */
gchar    *lc_fingerprint_compute (const gchar *const *items);   /* NULL only when items == NULL */
gboolean  lc_fingerprint_equal   (const char *a, const char *b);
```

**Reconciliation verdict (D15)** — one pure function, one truth table, both prune triggers in one pass:

```c
typedef enum { LC_RECONCILE_KEEP, LC_RECONCILE_DROP_NOT_LAUNCHER, LC_RECONCILE_DROP_FINGERPRINT } LcReconcileVerdict;
LcReconcileVerdict lc_store_reconcile_entry (const LcEntry *e,
                                             gboolean       id_is_launcher,
                                             gboolean       items_readable,
                                             const char    *computed_fp);   /* NULL when unreadable */
```

| id still a launcher | `items` readable | stored marker | computed vs stored | Verdict | Why |
|---|---|---|---|---|---|
| no | — | — | — | `DROP_NOT_LAUNCHER` | D12 — the id is not a launcher at all |
| yes | **no** | any | — | **`KEEP`** | Fail-safe: xfconf unreadable, key missing, or unexpected value type is our failure, not evidence about the user's launcher |
| yes | yes | absent or malformed | — | **`KEEP`**, adopt computed fp at next save | Legacy/hand-edited entry; healed silently rather than costing the user a colour |
| yes | yes | present | equal | `KEEP` | Same launcher |
| yes | yes | present | differ | `DROP_FINGERPRINT` | Recycled id now holds a different launcher |

**RF-8 now has two independent, complementary triggers.** D12 catches "this id is no longer a launcher"; D14 catches "this id is still a launcher, but a different one". Neither subsumes the other: D12 alone cannot see a launcher→launcher reassignment, and D14 alone cannot see a launcher→clock reassignment because there is no `items` list to fingerprint. Both run in the same load-time pass over the same fetched table, and again after each plugin-destroy debounce.

## Generated Rule Template and State Colours (D17–D19)

Every entry emits exactly three rules:

```css
#launcher-13 #launcher-arrow        { background-image: none; background-color: rgba(233,30,140,0.85); border-radius: 6px; margin: 2px; }
#launcher-13 #launcher-arrow:hover  { background-color: rgba(238,71,163,0.88); }
#launcher-13 #launcher-arrow:active { background-color: rgba(158,20,95,0.90); }
```

### Which rule carries which property (invariant)

| Property | Rule | Why |
|---|---|---|
| `border-radius`, `margin` | **Base only** | Geometry, not state. Corner rounding does not change on pointer enter or press. Repeating it would be three declarations that must always agree, and once RF-7 lets the user change the radius that is three chances to desync and an ambiguous target for any "the radius is R" assertion |
| `background-image: none` | **Base only** (see the caveat below) | A suppression of the theme's opaque image, not a state-dependent value |
| `background-color` | **All three**, and only this | The one genuinely state-varying property |

**Invariant: state rules carry colour only.** The generator and the record parser both depend on it, and it gives any future property an obvious home — geometry and suppressions go on the base rule, state-varying values go on the state rules.

#### Caveat on `background-image: none` — not fully verified

Two independent reasons say one base-rule declaration suffices in every state:

1. **Specificity.** `#launcher-<id> #launcher-arrow` carries two ID selectors. Themes style buttons by element, class and pseudo-class, never by the panel's per-instance ids, so our base rule outranks a theme `button:hover` rule on specificity alone.
2. **Provider priority.** Ours is `APPLICATION` (600), the theme is `THEME` (200).

Whichever of the two GTK3 weighs first, the base rule wins — which is why the recommended shape is very likely right.

**But it is not verified, and the spike does not close it.** `docs/spike-findings.md` ran the `background-image: none` probe only in the **non-hover** state (Adwaita, screen/`APPLICATION` → `rgba(233,29,140,0.85)`); its Adwaita hover row is explicitly moot "since nothing painted". Whether Adwaita or another common theme sets a *distinct* `background-image` for `:hover` or `:active` was never inspected, and I could not read Adwaita's stylesheet or the GTK3 cascade documentation from this context to settle it on paper.

So: keep `background-image: none` on the base rule only, and **verify it on the live panel**. If a common theme does paint a distinct prelight image, the state rules need their own `background-image: none` and this template changes — the invariant above would then read "state rules carry colour and image suppression". This is on the H0-b live checklist, not assumed away.

`:active` is the CSS pressed state. It is explicitly **not** "highlight the launcher whose window is focused" — that reading would require matching the launcher's desktop file against the active window's `WM_CLASS`, which is out of scope per the PRD's non-objectives.

### Selector construction is the sole producer (D18)

```c
typedef enum { LC_STATE_NORMAL, LC_STATE_HOVER, LC_STATE_ACTIVE } LcState;
gchar *lc_id_selector_new (gint id, LcState state);   /* the ONLY way a selector is produced */
```

GTK3 permits a state pseudo-class only on the last element of a selector chain, so `#launcher-13 #launcher-arrow:hover` is correct and `#launcher-13:hover #launcher-arrow` matches nothing and fails silently — the same trap as `#launcher-button`. The function appends the pseudo-class to the trailing element itself, so the inverted form cannot be written at a call site. No call site concatenates `":hover"`, and a unit test asserts the output never contains a pseudo-class followed by a descendant combinator.

### State colour derivation (D19)

```c
LcColor lc_color_derive_state (const LcColor *base, LcState state);   /* pure, no GTK */
```

Constants live together at the top of `src/core/lc-color.c`, in the D7 style:

| Constant | Value | Role |
|---|---|---|
| `LC_STATE_HOVER_MIX` | `0.18` | Colour blend fraction toward the target |
| `LC_STATE_ACTIVE_MIX` | `0.32` | Same, stronger — pressed reads as a larger change |
| `LC_STATE_HOVER_ALPHA_MIX` | `0.20` | Alpha blend fraction toward opaque |
| `LC_STATE_ACTIVE_ALPHA_MIX` | `0.35` | Same, stronger |
| `LC_STATE_LIGHT_PIVOT` | `0.85` | Above this luminance, hover goes darker instead of lighter |
| `LC_STATE_DARK_PIVOT` | `0.15` | Below this luminance, active goes lighter instead of darker |

Algorithm:

1. `L = 0.2126·r + 0.7152·g + 0.0722·b` on the stored sRGB values. Deliberately the cheap weighted average, not full linearisation — we need a perceptual *direction*, not colorimetric accuracy.
2. Pick the blend target so there is always headroom: hover targets white unless `L > LC_STATE_LIGHT_PIVOT`, in which case it targets black; active targets black unless `L < LC_STATE_DARK_PIVOT`, in which case it targets white.
3. Per channel: `out = base + (target − base) · MIX`.
4. Alpha: `out.a = base.a + (1 − base.a) · ALPHA_MIX`. This is a no-op at full opacity, so an opaque colour changes only in lightness and the user's transparency choice is preserved wherever it is meaningful.

Behaviour at the four extremes, which is what the tests pin:

| Base | Hover | Active | Why it works |
|---|---|---|---|
| Opaque mid-tone | Lighter, alpha unchanged | Darker, alpha unchanged | The ordinary case |
| Pure white | Slightly darker (light pivot) | Darker still | A lightness bump would be a no-op; both states stay distinct and ordered |
| Pure black | Lighter | Lighter still (dark pivot) | Cannot go darker than black. The pressed state is a larger change, so the two remain distinguishable even though both move the same way |
| `alpha ≈ 0.05` | Alpha rises to ≈ 0.24 | Alpha rises to ≈ 0.38 | Without the alpha blend the composited difference would be ~1 % and hover would still be invisible, reinstating the exact regression D17 fixes |

The invariant the tests assert over a sampled colour grid: base, hover and active are pairwise distinct by at least a minimum composited delta. That single property catches every degenerate case at once, including any future constant tweak.

## Configuration Formats

`~/.config/xfce4-launcher-colors/colors.css` — generated-only. Each entry is a fixed record: a marker comment line, then the rule, in the agreed shape:

```css
/* xfce4-launcher-colors — generated file, do not edit by hand. schema=2 */

/* id=12 fp=a3f1c8 */
#launcher-12 #launcher-arrow {
  background-image: none;
  background-color: rgba(233,30,140,0.85);
  border-radius: 6px;
  margin: 2px;
}
#launcher-12 #launcher-arrow:hover  { background-color: rgba(238,71,163,0.88); }
#launcher-12 #launcher-arrow:active { background-color: rgba(158,20,95,0.90); }
```

**Parsing.** The parser is record-oriented, which is affordable because we are the only writer and the layout is fixed. **A record is one marker line plus every rule that follows it until the next marker line or EOF** — not "the next three rules". That framing is what makes the three-rule entry work without touching the parser, and it stays correct if a fourth state is ever added: the marker is the delimiter, the rule count is not. Blank lines are ignored.

Within a record:

- The marker must match `/* id=<digits> fp=<1–16 hex> */`.
- Every rule in the record must carry the same ancestor id as the marker. A record whose rules disagree with their marker is malformed and is skipped whole — a partially applied entry is worse than none.
- **State rules carry colour only.** The base rule owns geometry (`border-radius`, `margin`) and the theme-image suppression (`background-image: none`); `:hover` and `:active` carry exactly one declaration, `background-color`. The parser rejects a state rule carrying anything else, so a generator that starts duplicating `border-radius` fails its own round-trip test rather than silently writing three values that can desync under RF-7.
- Only the base rule's `background-color` is read back into the store; the `:hover` and `:active` colours are **derived, never parsed**. They are outputs of `lc_color_derive_state()`, so re-deriving on render is both simpler and self-healing if the constants change. A hand-edited hover colour is silently overwritten at the next save, which the banner already warns about.

A rule whose marker is **absent or malformed** is not skipped — the rule is still read and the entry is kept with an empty fingerprint, which D15 heals at the next save. Nothing has shipped, so there is no migration to write; this rule exists because a truncated or hand-edited file must not cost the user their colours. The banner carries `schema=2`.

Rejected: a separate `colors.ini` + generated CSS — two files that desync, and the PRD names `colors.css` as the persistence file. Also rejected: keeping the whole rule on one line, which the agreed marker shape supersedes.

`~/.config/xfce4-launcher-colors/settings.ini` — `GKeyFile`, global style re-emitted into every rule so `colors.css` is self-contained:

```ini
[style]
corner-radius=6   # clamped 0–12 (RF-7)
margin=2
```

Autoload (RF-6): xfconf channel `xsettings`, key `/Gtk/Modules`, bare module name `xfce4-launcher-colors`. The backend normalizes string **and** string-array key shapes to `gchar**` and preserves the original shape on write.

## File Changes

| File | Action | Purpose |
|---|---|---|
| `meson.build` + `meson_options.txt` | Create | Project, `c_std=c11`, GPL-2.0-or-later, `provider_priority` combo option (D7) |
| `src/core/lc-guard.{c,h}` | Create | `gboolean lc_guard_should_activate (const char *prgname);` |
| `src/core/lc-id.{c,h}` | **Modify** — already on `feat/launcher-colors-01-core` (PR #1, unmerged) | `lc_id_selector_new` gains the `LcState` parameter (D18). Follow-up commit on that branch before merge |
| `src/core/lc-color.{c,h}` | **Modify** — same branch/PR | Adds `LcState`, `lc_color_derive_state()` and the six blend constants (D19). Follow-up commit on that branch before merge |
| `src/core/lc-css.{c,h}` | **Modify** — same branch/PR | `lc_css_render_rules()` currently emits **one** rule per entry; it must emit three (D17). Follow-up commit on `feat/launcher-colors-01-core` before merge, **not a new module** |
| `docs/spike-findings.md` | Create | Slice-2 spike record and citation for D7's priority verdict and D17's occlusion and hover findings, including the compared PNGs |
| `src/core/lc-fingerprint.{c,h}` | Create | `lc_fingerprint_compute` / `lc_fingerprint_equal` — pure, takes the desktop-file list as a plain `gchar**` (D14) |
| `src/core/lc-store.{c,h}` | Create | `LcStore` load/save/set/unset/has/list/**prune**/**reconcile**/render; marker-record parser; atomic save; holds the private definition of `LcDocument` |
| `src/core/lc-document.h` | Create | Opaque `LcDocument` + `lc_document_css`/`lc_document_free`. No constructor other than `lc_store_render()` (D16) |
| `src/core/lc-settings.{c,h}` | Create | `settings.ini` read/write, clamping |
| `src/core/lc-paths.{c,h}` | Create | XDG path resolution |
| `src/core/lc-autoload.{c,h}` | Create | `LcAutoloadBackend` vtable + pure list transforms (D13) |
| `src/glue/module.c` | Create | `gtk_module_init` / `gtk_module_exit` |
| `src/glue/lc-provider.{c,h}` | Create | **Sole** owner of priority and attach scope (D7), and the only file that holds a `GtkCssProvider` or calls `load_from_*`. Accepts `const LcDocument *` only (D16) |
| `src/glue/lc-hook.{c,h}` | Create | Emission hook, `lc_hook_resolve_plugin()` (D4), registry |
| `src/glue/lc-menu.{c,h}` | Create | Item insertion, visibility refresh (D9/D10) |
| `src/glue/lc-dialogs.{c,h}` | Create | `GtkColorChooserDialog` (RF-2), style dialog (RF-7) |
| `src/glue/lc-autoload-xfconf.c` | Create | `LcAutoloadBackend` implementation |
| `src/cli/main.c` | Create | `set\|unset\|list\|--enable\|--disable` (RF-10) — links core + xfconf backend, **not GTK**, never dlopens the module. `set` computes and writes the fingerprint through the same core path as the module, so a CLI-written entry is indistinguishable from a menu-written one |
| `tests/test-*.c` | Create | One binary per core unit |
| `tests/test-boundaries.sh` | Create | Grep guard wired as a `meson test`: `GtkCssProvider`/`load_from_*` confined to `lc-provider.c` (D16); no GTK include under `src/core/` |
| `docs/manual-verification.md` | Create | Live-panel checklist (things `meson test` cannot reach) |
| `po/`, `debian/`, `install.sh` | Create | Slices 7–8 |
| `openspec/config.yaml` | Modify | `strict_tdd: false → true` in slice 1 |

Meson targets: `static_library('lc-core', deps: [glib, gio])` · `shared_module('xfce4-launcher-colors', link_with: lc_core, deps: [gtk3, libxfce4panel, libxfce4util, xfconf], install_dir: gtk3_libdir/'gtk-3.0/modules')` · `executable('xfce4-launcher-colors', link_with: lc_core, deps: [glib, xfconf])` · `tests/` linking `lc_core` only.

## Interfaces (core → glue boundary)

```c
gboolean  lc_guard_should_activate (const char *prgname);
gchar    *lc_id_selector_new       (gint id, LcState state);  /* sole selector producer (D18);
                                                                 pseudo-class on the LAST element only */
gboolean  lc_color_parse           (const char *text, LcColor *out);
LcColor   lc_color_derive_state    (const LcColor *base, LcState state);   /* hover/active (D19) */
gchar    *lc_fingerprint_compute   (const gchar *const *items);       /* pure; glue supplies the list */
LcDocument *lc_store_render        (LcStore *s, const LcStyle *st);   /* full document, markers included;
                                                                         SOLE constructor of LcDocument (D16) */
const char *lc_document_css        (const LcDocument *d);             /* borrowed */
void        lc_document_free       (LcDocument *d);
gboolean  lc_store_set             (LcStore *s, gint id, const LcColor *c, const char *fp);
gboolean  lc_store_has             (LcStore *s, gint id);     /* drives RF-5 visibility */
guint     lc_store_prune           (LcStore *s, const gint *live_ids, gsize n);
LcReconcileVerdict lc_store_reconcile_entry (const LcEntry *e, gboolean id_is_launcher,
                                             gboolean items_readable, const char *computed_fp);
gboolean  lc_store_is_dirty        (LcStore *s);   /* gates the write-back; FALSE ⇒ no-op reconcile, no write (D16) */
gboolean  lc_store_save            (LcStore *s, const char *path, GError **error);
gchar   **lc_autoload_add          (const gchar *const *modules);   /* idempotent */
```

The provider side of the boundary — the **only** three functions in the tree that can reach a `GtkCssProvider`, which is a file-scope `static` in `lc-provider.c` with no accessor (D16):

```c
/* src/glue/lc-provider.h */
void lc_provider_attach       (GdkScreen *screen, const LcDocument *doc);  /* once, at init */
void lc_provider_set_document (const LcDocument *doc);                     /* every reload */
void lc_provider_detach       (void);
```

Both CSS-accepting functions take `const LcDocument *` and nothing else, so no call site can attach or reload without having rendered a reconciled store first. There is no `const char *` overload, by design.

Glue owns every `Gtk*`/`Xfce*` type; core headers include no GTK header. Enforced by the meson dependency list, not by convention.

## Data Flow — apply a colour

    menu item → GtkColorChooserDialog → LcColor
      → glue reads /plugins/plugin-<id>/items → lc_fingerprint_compute    (fresh fp, D14)
      → lc_store_set (id, colour, fp)
      → lc_store_render → LcDocument → lc_provider_set_document           (visual, instant, RF-3)
      → lc_store_save (write+rename, atomic)                              (persistence, RF-4)
      → lc_menu_refresh_visibility (RF-5)

The RF-7 style change and the `GFileMonitor` reload take the same shape: whatever triggered them, they end at `lc_store_render()` → `lc_provider_set_document()`, because that is the only signature the provider accepts (D16).

The fingerprint is recomputed on **every** write, never carried over from the loaded entry, so it always records the launcher's state at the moment the user chose the colour. If `items` cannot be read at that moment the colour is still stored, with an empty fingerprint that the next successful load heals — the same fail-safe direction as D15.

Visual apply happens **before** the disk write: a failed write leaves the colour visible plus one warning, rather than silently discarding the user's action. A `GFileMonitor` on `colors.css` (250 ms debounce, event-driven, no polling) makes CLI edits apply live; fallback is `xfce4-panel -r`.

## Testing Strategy

| Layer | What | How |
|---|---|---|
| Unit (`meson test`, headless, no GTK, no panel) | guard predicate; id/selector; colour round-trip; CSS generation incl. a regression test asserting the literal `#launcher-arrow` and that `#launcher-button` never appears; marker-record parse/emit round-trip; store load/save/prune; malformed-file degradation; settings clamping; autoload transforms via fake backend | One test binary per core unit, fixtures under `tests/data/` |
| Unit — state colours (D19) | the four extremes (opaque mid-tone, pure white, pure black, `alpha ≈ 0.05`); the pairwise-distinctness invariant over a sampled colour grid; alpha never exceeds 1.0 and never decreases; a fully opaque base keeps `a = 1.0`; pivot boundary cases at exactly `0.15` and `0.85` | `tests/test-color.c` (existing binary on PR #1, extended) |
| Unit — rule template (D17/D18) | every entry renders exactly three rules; the base rule contains `background-image: none`; the `:hover` and `:active` selectors end in the pseudo-class and **no** output ever matches a pseudo-class followed by a descendant combinator; the literal `#launcher-arrow` regression assertion still holds across all three; **property placement — `border-radius` and `margin` appear exactly once, on the base rule, and each state rule carries `background-color` and nothing else**; changing the RF-7 radius changes exactly one declaration | `tests/test-css.c` (existing binary on PR #1, extended) |
| Unit — fingerprint (D14) | determinism across repeated calls; **order-insensitivity** (same set, shuffled → same fp); sensitivity (one element replaced → different fp); empty/`NULL`/whitespace-only elements; length-framing (`["ab","c"]` vs `["a","bc"]` must differ); non-UTF-8 and embedded-separator bytes; output is exactly 6 lowercase hex chars | `tests/test-fingerprint.c`, no xfconf, no GTK |
| Unit — reconciliation (D15) | the full 5-row truth table, one case per row, with `DROP` asserted **only** for a computed genuine mismatch and `KEEP` asserted for unreadable `items`, absent marker, and malformed marker | `tests/test-reconcile.c` — this is the fail-safe direction under test |
| Unit — record parser (D17 shape) | a three-rule record round-trips; a record is delimited by the next marker, not by a rule count (a four-rule record still parses); a record whose rules disagree with their marker id is skipped whole; `:hover`/`:active` colours are re-derived on render rather than read back | `tests/test-store.c` |
| Unit — startup ordering (D16) | `lc_store_render()` after a reconciliation that dropped entries contains **no** rule or marker for the dropped ids; `lc_store_is_dirty()` is `FALSE` after a reconciliation that dropped nothing and `TRUE` after one that did; a reconciliation that could not run leaves every entry present in the rendered document | `tests/test-reconcile.c` — asserts on `lc_document_css()` and the dirty flag, so the "provider must never see stale rules" guarantee is checked without GTK |
| Build guard — provider containment (D16) | `gtk_css_provider_`, `load_from_data`, `load_from_file` and `GtkCssProvider` appear **only** in `src/glue/lc-provider.c`; no header exposes the provider | `tests/test-boundaries.sh`, a grep assertion wired as a `meson test`. Second line of defence — the opaque `LcDocument` is the actual barrier |
| Manual (live panel) | module load via `/Gtk/Modules`; guard no-ops in another GTK3 app; hook finds real launchers; items appear above "Propiedades"; **paints over Adwaita specifically, which needs `background-image: none` (D17)**; **hover and pressed feedback both still visible on a coloured launcher**; **UNVERIFIED — does the single base-rule `background-image: none` also suppress the theme image in `:hover` and `:active`? Inspect a coloured launcher under Adwaita in both states; if a distinct prelight image paints, the state rules need their own `background-image: none` and D17's template changes**; < 100 ms; RSS delta < 1 MB; logout survival; `--disable` restores native panel; **recycled-id scenario: colour a launcher, unload the module, remove that launcher, add a different one that takes the id, reload — the new launcher must render uncoloured, and must never flash the stale colour first (D16)**; a session that prunes nothing leaves `colors.css` mtime unchanged; reconciliation of 22 entries within RNF-3 | `docs/manual-verification.md` checklist |
| CI (slice 7) | Build + `meson test` on 4.16 / 4.18 / 4.20 containers; lintian | GitHub Actions matrix |

## Threat Matrix

| Boundary | Applicability | Design response | Planned RED tests |
|---|---|---|---|
| Process integration — the `.so` loads into **every** GTK3 process | **Applicable** | `lc_guard_should_activate` runs first; nothing allocated otherwise | `"xfce4-panel"`→true; `"gedit"`, `NULL`, `""`, `"xfce4-panel-wrapper"`, `"Xfce4-Panel"` → false |
| Untrusted config input — `colors.css` / `settings.ini` are user-writable and feed a CSS provider | **Applicable** | D8: parse → store → render; raw file never reaches the provider. Marker records that do not match in full are skipped; an absent or malformed marker keeps the entry (D15) | Fixtures: unterminated rule, `@import`, negative/huge id, non-UTF-8 bytes, 10 MB file, radius `999` → clamped; marker with a non-hex fp, an over-long fp, a mismatched `id=`, a marker with no rule after it, a rule with no marker |
| xfconf read of `/plugins` for reconciliation | **Applicable** | Values are panel-owned but read defensively: type-checked before use, never trusted to be a string array; an unexpected type is `items_readable = FALSE`, which KEEPS the entry | Fake properties table: key missing, wrong `GType`, empty array, array with `NULL` elements |
| Subprocess — `xfce4-panel -r` from the CLI | **Applicable** | `g_spawn_async` with an argv **vector**; never `g_spawn_command_line_*`, never a shell string | Builder returns a vector; asserts no `sh -c`; non-zero exit warns without partial state |
| xfconf key mutation — `/Gtk/Modules` | **Applicable** | Pure add/remove over a normalized `gchar**`; idempotent; shape preserved | Key absent; key holds other modules (must survive); already present (no duplicate); string vs string-array shape |
| Documentation-like paths | N/A | No file-classification or execution boundary in this change | — |
| Git repository selection / commit state / push state / PR commands | N/A | No VCS or PR automation in this change | — |

## Migration / Rollout

No data migration — greenfield, and `colors.css` carries `schema=2` for future format changes. The marker-free draft shape needs no migration because nothing has shipped; a marker-free entry is simply kept and healed on the next save (D15). Rollout follows the proposal's Feature Branch Chain on `feat/launcher-colors`. Slice 1 (`meson.build` + `src/core/` + `tests/`) links no GTK, so it can land and go green before the panel dev headers exist — a partial relaxation of the proposal's blocking prerequisite, which still binds slice 3 onward `[+]`. Rollback: `--disable` → uninstall → revert the slice PR.

## Open Questions

Carried deliberately; **not** decided here. `docs/spike-findings.md`
(Slice 2, H0-b standalone-harness portion) records empirical evidence
toward the first three items below — priority winner, paint/hover result
per theme, and a harness-only (not live-panel-confirmed) map-hook
ordering observation. None of the items below are checked off: the
live-panel confirmation step has not run.

- [x] **Provider priority** — **CLOSED by the slice-2 spike** (`docs/spike-findings.md`): `APPLICATION` (600) wins at both screen and widget scope, and is now the confirmed default in `meson_options.txt`. `USER` is not needed, so the user keeps the last word through their own `gtk.css`. Widget-level outranking screen-level at equal priority was confirmed at the same time.
- [ ] **Does the background visibly paint over real themes without breaking hover?** The *cause* is now known and addressed, so this is no longer open-ended: Adwaita never painted because its `GtkButton` sets an opaque `background-image`, and a flat background suppressed the theme's prelight. D17 fixes both. What remains open is confirmation **on a live panel** rather than in the spike harness, and one sub-question is genuinely unverified rather than merely unconfirmed:
  - **Does one base-rule `background-image: none` suppress the theme image in `:hover` and `:active` too?** The spike probed it only in the non-hover state; its Adwaita hover row is moot because nothing painted. Specificity (two IDs) and priority (600 vs 200) both say yes, but neither Adwaita's stylesheet nor the GTK3 cascade order was inspected. If a common theme paints a distinct prelight image, the state rules need their own `background-image: none` and D17's template changes.
  - Whether the derived state colours read correctly against a real theme.
- [ ] `map` hook timing and filtering cost measured in a live panel (H0-b); confirms D4's "no ancestor walk needed".
- [ ] Menu-caching code path re-confirmed on 4.16.0 (verified on 4.18.0 and `master`).
- [ ] Distro-patched `libxfce4panel` headers vs upstream, after the dev package lands.
- [ ] Re-confirm code citations against `gitlab.xfce.org` rather than GitHub mirrors.
