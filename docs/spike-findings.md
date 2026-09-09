# Spike Findings — Slice 2 (H0-b), standalone-harness portion

**Status: empirical evidence from a standalone GTK3 harness only.** No code
here loaded into the live `xfce4-panel` process, no xfconf key was read or
written, and no running panel was disturbed. The live-panel confirmation
step (tasks.md 2.2, 2.3, 2.4) is **deferred** pending explicit user consent
and is NOT covered by this document. `design.md`'s Open Questions items are
**not** checked off — see "What remains unanswered" below.

Harness source: `spike/harness.c`. Build and run commands, exact output,
and generated PNGs are reproduced in full below so results are verifiable
without re-running the program.

## Reproduced structure

Verified identical across xfce4-panel 4.16.0, 4.18.0, and `master`
(`launcher_plugin_init()`):

- an outer container named `launcher-13` (`GtkEventBox` stand-in for
  `XfcePanelPlugin`);
- a `GtkButton` inside it whose name is set to `launcher-button` and then
  immediately overwritten to `launcher-arrow` (upstream naming bug,
  reproduced verbatim);
- a widget-level `GtkCssProvider` on the button's own style context at
  `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION` (600) loading
  `#launcher-arrow { min-height: 0; min-width: 0; }` — the panel's real,
  always-present competing provider.

## Build and run commands used

Direct `gcc` build (no meson target — kept out of the install tree per
task 2.7; `spike/` is never referenced from `meson.build`):

```
gcc -std=c11 -Wall -Wextra $(pkg-config --cflags gtk+-3.0) \
    -o spike/harness spike/harness.c $(pkg-config --libs gtk+-3.0) -lm
```

Build result: clean, zero warnings under `-Wall -Wextra`.

Run commands (against the existing X session's `DISPLAY`; the harness
opens no visible window — it renders exclusively into `GtkOffscreenWindow`
and in-memory Cairo surfaces):

```
DISPLAY=:0.0 ./spike/harness                    # system default theme
GTK_THEME=Adwaita DISPLAY=:0.0 ./spike/harness  # Adwaita, this process only
```

## Q1 — which provider priority wins

Matrix tested: our rule `#launcher-13 #launcher-arrow { background-color:
rgba(233,30,140,0.85); border-radius: 6px; }` attached at
`APPLICATION` (600) and `USER` (800), at both **screen** scope
(`gtk_style_context_add_provider_for_screen`) and **widget** scope
(`gtk_style_context_add_provider` directly on the button), always against
the panel's real widget-level `APPLICATION` min-size rule.

### System default theme (`Mint-Y-Dark-Pink`)

| Scenario | Resolved background | Verdict |
|---|---|---|
| screen / APPLICATION (600) | `rgba(233,29,140,0.85)` | **OUR provider WINS** |
| screen / USER (800) | `rgba(233,29,140,0.85)` | **OUR provider WINS** |
| widget / APPLICATION (600) | `rgba(233,29,140,0.85)` | **OUR provider WINS** |
| widget / USER (800) | `rgba(233,29,140,0.85)` | **OUR provider WINS** |

(Green channel reads `29` instead of the nominal `30` — expected 8-bit
premultiplied-alpha rounding when the surface is unpremultiplied for
sampling, not a functional discrepancy.)

**Verdict for the system default theme: `APPLICATION` (600) is already
sufficient.** The panel's own competing rule only sets
`min-height`/`min-width`, never `background-color`, so it never actually
contends with our rule on that property — every tested priority/scope
combination applied cleanly.

### Adwaita (forced via `GTK_THEME=Adwaita`, this process only)

| Scenario | Resolved background | Verdict |
|---|---|---|
| screen / APPLICATION (600) | `rgba(241,240,238,1.00)` | our rule LOST |
| screen / USER (800) | `rgba(241,240,238,1.00)` | our rule LOST |
| widget / APPLICATION (600) | `rgba(241,240,238,1.00)` | our rule LOST |
| widget / USER (800) | `rgba(241,240,238,1.00)` | our rule LOST |

Neither priority nor scope mattered here — the value never changed from
the theme's own opaque button background. See "Diagnostic finding" below
for why, and for the fix that recovers a win.

### Q1b — genuine same-property conflict at equal priority

The panel's real rule never competes with ours on `background-color`, so
scenarios above cannot test D7's claim that "widget-level providers
outrank screen-level ones at equal priority." A dedicated scenario adds a
**decoy** widget-level rule directly on the button
(`#launcher-arrow { background-color: rgba(80,80,80,1.0); }`) at the same
`APPLICATION` (600) priority as our screen-level provider, so the two
genuinely compete over the same property:

- System default theme: resolved `rgba(80,80,80,1.00)` — **the
  widget-level decoy wins.**
- Adwaita: resolved `rgba(241,240,238,1.00)` — theme background wins over
  both (see diagnostic below); the decoy itself was also occluded, so this
  run does not add independent confirmation under Adwaita.

**Verdict: CONFIRMED under the system default theme.** D7's design
rationale — a widget-level provider outranks a screen-level provider at
equal numeric priority — is empirically correct for this widget/property
shape, at least under `Mint-Y-Dark-Pink`. Production's `lc-provider.c`
must not rely on numeric priority alone to guarantee it beats a
hypothetical widget-level competitor at the same priority; scope
(screen vs widget) also decides the outcome.

### Diagnostic finding — why Adwaita lost regardless of priority/scope

A supplementary probe (not part of the assigned Q1 rule) added
`background-image: none;` to our rule:

```
#launcher-13 #launcher-arrow {
  background-color: rgba(233,30,140,0.85);
  background-image: none;
  border-radius: 6px;
}
```

Result under Adwaita, screen/APPLICATION: `rgba(233,29,140,0.85)` — **our
provider wins once `background-image` is cleared.**

**Interpretation:** Adwaita's `GtkButton` CSS sets an opaque
`background-image` (a gradient) that paints over `background-color`
regardless of the overriding provider's priority or scope, because
`background-image` and `background-color` are independent CSS layers —
setting one does not clear the other. This is not a provider-priority
problem at all; it is a rule-completeness problem. **Recommendation for
the real CSS rule template** (owned by `src/core/lc-css.c` per
`design.md` D1/D7, out of scope for this slice's allowed changes): the
generated rule must also set `background-image: none;`, or the override
will silently fail to paint under any theme whose button style uses a
background image (Adwaita is a common one; other themes were not tested
here). This is flagged in `design.md`'s Open Questions cross-reference
below and left for a design/task update in a later slice — no `src/`
change was made in this slice per scope.

## Q2 — does it visibly paint, and does hover survive

Captured via `GtkOffscreenWindow` + `gtk_widget_draw()` into an in-memory
`cairo_image_surface_t`, written with `cairo_surface_write_to_png()`. No
screen-capture tool and no compositor interaction were used. PNGs are
under `spike/out/`:

| Theme | Baseline | Painted (winning priority) | Hover |
|---|---|---|---|
| `Mint-Y-Dark-Pink` (system default) | `baseline-Mint-Y-Dark-Pink.png` | `painted-Mint-Y-Dark-Pink-application.png` | `hover-Mint-Y-Dark-Pink-application.png` |
| `Adwaita` (forced) | `baseline-Adwaita.png` | `painted-Adwaita-user.png` | `hover-Adwaita-user.png` |

Both themes tested (both installed and available on this machine; no
theme was untestable).

### `Mint-Y-Dark-Pink` result

- Baseline background: `rgba(51,51,56,1.00)` (dark, opaque theme default).
- Painted (screen/APPLICATION — winning priority for this theme):
  `rgba(233,29,140,0.85)`. Visually confirmed pink in
  `painted-Mint-Y-Dark-Pink-application.png` — **paints correctly.**
- Hover (`GTK_STATE_FLAG_PRELIGHT` forced): `rgba(233,29,140,0.85)` —
  **identical to the non-hover painted value.** Visually confirmed
  identical in `hover-Mint-Y-Dark-Pink-application.png`.

  **Finding: hover feedback does NOT read through.** Our rule's flat
  `background-color` at `APPLICATION` (600) priority outranks the theme's
  own `:hover`/`:prelight` background rule (typically at `THEME`, 200),
  so the button visually stops indicating hover once coloured. This is a
  real UX regression risk worth a design note, not merely a paint
  success/failure question.

### `Adwaita` (forced) result

- Baseline: `rgba(241,240,238,1.00)` (Adwaita's default opaque button
  background).
- Painted (fallback to screen/USER since no screen-level priority won for
  this theme with the assigned rule — see Q1 above): `rgba(241,240,238,1.00)`
  — **did not paint** with the exact rule specified for Q1. The
  background-image diagnostic above shows it WOULD paint with one
  additional declaration (`background-image: none;`).
- Hover: `rgba(241,240,238,1.00)` — unchanged (moot, since nothing painted
  in the first place under Adwaita with the assigned rule).

## Q3 — map-hook targeting (harness evidence only, NOT a live-panel confirmation)

A global `g_signal_add_emission_hook()` on `GtkWidget::map` was installed
for the whole harness run and filtered to the two widgets of interest.
Observed order, consistent across all repeated scenario builds in both
theme runs:

```
map fired: type=GtkButton    name="launcher-arrow" addr=<...>
map fired: type=GtkEventBox  name="launcher-13"    addr=<...>
```

**Observation: the child button's `map` fires BEFORE the outer
container's `map`**, for this exact container/child shape
(`GtkEventBox` containing one `GtkButton`, both shown via
`gtk_widget_show_all()` on the top-level offscreen window).

**This does not confirm design.md D4.** D4 assumes the plugin GObject's
own `map` fires (so `lc_hook_resolve_plugin()` can match it directly, no
ancestor walk). In this harness, the **child fires first**, which is the
opposite of what D4's rationale describes for its no-ancestor-walk
assumption ("Containers map before children"). Two important caveats
before drawing any conclusion for production:

1. `GtkEventBox` is a generic stand-in for `XfcePanelPlugin`; the real
   `XfcePanelPlugin` may have different internal show/map sequencing
   (e.g. it may explicitly call `gtk_widget_show()` on itself before
   adding/showing children, rather than relying on a single
   `gtk_widget_show_all()` on an ancestor as this harness does).
2. `gtk_widget_show_all()` recurses top-down setting the visibility flag,
   but the GTK mapping pass that actually emits `map` runs bottom-up in
   many GTK3 container implementations for size-negotiation reasons —
   which is consistent with what was observed here, but is a GTK
   container/widget implementation detail, not something this harness's
   simplified container proves for `XfcePanelPlugin` specifically.

**Conclusion: flagged as a discrepancy to re-check in the live-panel
step, not as a refutation of D4.** `lc_hook_resolve_plugin()`'s ancestor
walk fallback (already designed as a compiled-in fallback, never removed)
covers exactly this scenario if the live panel behaves the same way as
this harness. No production code was changed based on this finding.

## Provider priority default (task 2.6)

**Winning priority: `application`.** Confirmed against the real panel's
competing widget-level rule (which never touches `background-color`),
`APPLICATION` (600) at screen scope already wins on the system default
theme in every tested case. There is no evidence in this harness that
escalating to `USER` (800) is necessary against xfce4-panel's own CSS.
`meson_options.txt`'s `provider_priority` default was already set to
`application`; only its comment was updated to record that this is now
the empirically confirmed value (H0-b), not a placeholder.

The Adwaita `background-image` gap identified above is priority-scope
independent (no priority/scope combination fixed it) and is therefore
irrelevant to the `provider_priority` option itself — it requires an
additional CSS declaration in the generated rule (see the recommendation
above), tracked as a follow-up, not a `provider_priority` change.

## What remains unanswered until the live-panel step runs

- Whether the plugin GObject's own `map` fires first against the REAL
  `XfcePanelPlugin`/launcher-button pair (Q3 above is harness-only and,
  if anything, points the opposite direction from D4's assumption for
  this simplified container — needs live confirmation).
- Whether the background visibly paints over the actual Xubuntu default
  theme (not tested here — only the machine's actual current theme,
  `Mint-Y-Dark-Pink`, and `Adwaita`, were available/tested).
- Hook-callback cost against RNF-2 (cheap filter path) and RNF-3
  (<10 ms) measured with the panel's real launcher count (task 2.4) —
  this harness has no live launcher population to measure against and
  did not attempt a synthetic substitute.
- Whether hover-feedback loss (found here under the system default
  theme) reproduces identically against the real panel and real themes,
  and whether it is acceptable UX or needs a design response (e.g. a
  `:hover` rule of our own).
- Whether the `background-image: none;` gap reproduces against Adwaita
  and other themes when loaded into the real panel, and whether other
  untested themes have analogous gaps (only two themes were available on
  this machine).
- `design.md`'s Open Questions items remain unchecked; only this
  document is new evidence toward resolving them.
