# Launcher Color Persistence Specification

## Purpose

Defines the `colors.css` schema, its CSS selector contract, its id+fingerprint
binding, read/write behavior, and reconciliation rules across sessions,
launcher moves, and launcher removal — including the recycled-id case where
`xfce4-panel` reassigns a removed launcher's unique id to an unrelated plugin.

## Requirements

### Requirement: Generated Rule Shape and Theme Override (RF-3, RF-4)

For every colored launcher, the system MUST generate exactly three CSS rules
under the descending selector `#launcher-<id> #launcher-arrow`: a base rule,
a `:hover` rule, and an `:active` rule. The base rule MUST set
`background-image: none` before `background-color`, because Adwaita's
`GtkButton` sets an opaque `background-image` that occludes
`background-color` and would otherwise render nothing at all. The system
MUST NOT generate rules against `#launcher-button`, which matches no widget
at runtime.
(Previously: a single rule was generated per entry, without
`background-image: none`; slice-2 spike findings against Adwaita showed this
insufficient — the color rendered nothing at any provider priority or scope.)

#### Scenario: Base rule neutralizes the theme's opaque background-image

- GIVEN a launcher with unique id 13 and color `rgba(233,30,140,0.85)`
- WHEN the system generates its base CSS rule for that launcher
- THEN the rule includes `background-image: none` together with
  `background-color: rgba(233,30,140,0.85)`
- Unit-testable under Meson: yes — pure string-generation logic; a generator
  that omits `background-image: none` MUST fail this scenario.

#### Scenario: Three rules are generated per colored launcher

- GIVEN a launcher with unique id 13 and an assigned color
- WHEN the system generates its CSS output for that launcher
- THEN exactly three rules exist: a base `#launcher-13 #launcher-arrow` rule,
  a `#launcher-13 #launcher-arrow:hover` rule, and a
  `#launcher-13 #launcher-arrow:active` rule
- Unit-testable under Meson: yes.

#### Scenario: A single-level selector is never produced

- GIVEN the CSS generator is asked to target any launcher id
- WHEN it produces any of the three rules
- THEN each rule always includes both the `#launcher-<id>` ancestor and the
  `#launcher-arrow` descendant, never `#launcher-<id>` or `#launcher-button` alone
- Unit-testable under Meson: yes.

### Requirement: Distinguishable Hover and Pressed Feedback (RF-3)

A colored launcher MUST retain observable hover and pressed feedback: the
`:hover` rule's `background-color` MUST differ from the base rule's
`background-color`, and the `:active` rule's `background-color` MUST differ
from both the base and the `:hover` `background-color`. The system MUST NOT
emit an identical color for all three states. The specific color transform
used to derive the hover and active colors is not specified here.

#### Scenario: Hover and active colors differ from the base color

- GIVEN a launcher has been assigned a base color
- WHEN the system generates its hover and active rules
- THEN the hover `background-color` differs from the base `background-color`,
  and the active `background-color` differs from both the base and hover values
- Unit-testable under Meson: yes — pure comparison of generated color values.

#### Scenario: Identical colors across all three states is a failing case

- GIVEN a color generator produces the same `background-color` for the base,
  `:hover`, and `:active` rules of a launcher
- WHEN this output is checked against the requirement
- THEN the check fails, because indistinguishable states are non-conformant
- Unit-testable under Meson: yes — this is the regression-guard scenario for
  an implementation that reuses the base color unchanged for all three states.

### Requirement: Pseudo-Class Placement on the Selector Chain (RF-3, RF-4)

Every state-specific rule (`:hover`, `:active`) MUST attach its pseudo-class
to the last element of the selector chain — the descendant, e.g.
`#launcher-<id> #launcher-arrow:hover` — and MUST NOT attach it to the
ancestor, e.g. `#launcher-<id>:hover #launcher-arrow`. GTK3 resolves a
pseudo-class against the element it is attached to; attaching it to the
ancestor produces a selector that silently matches nothing, the same failure
shape as the `#launcher-button` prohibition above.

#### Scenario: Pseudo-class is attached to the descendant

- GIVEN the system generates a `:hover` rule for launcher id 13
- WHEN the selector is inspected
- THEN it reads `#launcher-13 #launcher-arrow:hover`, with the pseudo-class
  attached to `#launcher-arrow`
- Unit-testable under Meson: yes.

#### Scenario: Attaching the pseudo-class to the ancestor is never produced

- GIVEN the system generates any state-specific rule
- WHEN the selector is inspected
- THEN it never reads `#launcher-<id>:hover #launcher-arrow`, or any other
  form attaching the pseudo-class to the ancestor
- Unit-testable under Meson: yes — a generator producing this form MUST fail
  this scenario, mirroring the existing `#launcher-button` prohibition.

### Requirement: Desktop-File Fingerprint Binding (RF-4, RF-8)

The system MUST compute a fingerprint from a launcher's desktop-file list
(the panel's `items` property for that plugin instance) and MUST store this
fingerprint alongside the launcher id in every `colors.css` entry, as a
comment marker in the form `/* id=<id> fp=<fingerprint> */` immediately
preceding that entry's rule triplet (base, `:hover`, `:active`). The system
MUST recompute and rewrite this marker every time a color is set for that
launcher.

#### Scenario: Fingerprint marker is written immediately before the rule triplet

- GIVEN a launcher with id 12 and a given desktop-file list
- WHEN the user assigns a color to it
- THEN `colors.css` contains a `/* id=12 fp=<fingerprint> */` marker immediately
  followed by the base, `:hover`, and `:active` rules for `#launcher-12 #launcher-arrow`
- Unit-testable under Meson: yes — fingerprint computation and write logic, with the desktop-file list injected as input.

#### Scenario: Fingerprint is rewritten when the desktop-file list has changed

- GIVEN launcher 12's stored marker was computed from a previous desktop-file list
- WHEN a color is set again after that launcher's desktop-file list changed
- THEN the rewritten marker reflects the new fingerprint, not the stale one
- Unit-testable under Meson: yes.

### Requirement: Color Persistence Across Sessions (RF-4)

The system MUST write each assigned color, together with its id+fingerprint
marker, to `~/.config/xfce4-launcher-colors/colors.css`, and MUST restore
every entry that passes reconciliation (see below) when the module loads at
the start of a new session.

#### Scenario: Color survives logout/login

- GIVEN a launcher has a saved color and a matching fingerprint from a previous session
- WHEN the user logs back in and the module loads
- THEN the launcher shows the same background color without user action
- Manual verification only: requires a real logout/login cycle.

#### Scenario: colors.css round-trips through read and write

- GIVEN a `colors.css` file with N valid entries, each with its id, fingerprint marker, and color
- WHEN the system reads it and writes it back unmodified
- THEN the result contains the same N entries with equivalent selectors, markers, and colors
- Unit-testable under Meson: yes.

### Requirement: Fingerprint Mismatch Discards Entry at Load (RF-8)

At module load, the system MUST recompute each stored entry's fingerprint
from the launcher's current desktop-file list, read from the xfconf
`xfce4-panel` channel, and MUST discard any entry whose recomputed
fingerprint does not match its stored fingerprint. A discarded launcher
renders with no assigned background color.

#### Scenario: Recycled id with a mismatched fingerprint is discarded

- GIVEN `colors.css` has an entry for id 13 with fingerprint `fp=a3f1c8`, but xfconf
  now shows plugin id 13 as a launcher whose current desktop-file list yields `fp=9b2e01`
- WHEN the module loads and reconciles `colors.css`
- THEN the entry for id 13 is discarded and that launcher renders with no background color
- Unit-testable under Meson: yes, for the reconciliation function given mocked
  stored vs. recomputed fingerprints. Reading the live xfconf channel is glue,
  verified manually/by integration test.

#### Scenario: A matching fingerprint is retained and applied

- GIVEN `colors.css` has an entry for id 21 whose stored fingerprint matches the recomputed one
- WHEN the module loads
- THEN the entry is retained and its color is applied
- Unit-testable under Meson: yes.

### Requirement: Fingerprint Computation Failure Fail-Safe (RF-8, RNF-6)

If the system cannot compute a launcher's current fingerprint — because the
xfconf channel is unreadable, the relevant key is missing, or its value is of
an unexpected type — the system MUST keep the stored entry unmodified and
MUST NOT discard it. Only a fingerprint that was successfully recomputed and
genuinely differs from the stored value MAY cause an entry to be discarded.
A transient xfconf failure MUST NEVER wipe the user's stored colors.

#### Scenario: Unreadable xfconf channel keeps every entry

- GIVEN the xfconf `xfce4-panel` channel cannot be read (e.g. `xfconfd` unavailable) at module load
- WHEN the system attempts to recompute fingerprints for reconciliation
- THEN every stored entry is kept exactly as-is, and no entry is discarded because of this failure
- Unit-testable under Meson: yes, for the reconciliation function given a simulated read failure.

#### Scenario: A missing items key does not wipe the entry

- GIVEN a launcher's `items` key is absent from its xfconf properties
- WHEN the module attempts to recompute that launcher's fingerprint
- THEN the computation is treated as failed, not as an empty-list fingerprint, and the stored entry is kept
- Unit-testable under Meson: yes.

#### Scenario: Discarding on computation failure is non-conformant (regression guard)

- GIVEN fingerprint computation fails for a stored entry, for any reason — unreadable
  channel, missing key, or unexpected value type
- WHEN reconciliation runs
- THEN the entry MUST still be present and applied afterward; an implementation that
  discards it on failure violates this requirement, because only a successfully
  computed and genuinely mismatching fingerprint may discard an entry
- Unit-testable under Meson: yes — this scenario MUST fail against any
  implementation that treats "cannot compute" the same as "mismatched".

### Requirement: Absent or Malformed Fingerprint Marker (RF-8)

An entry in `colors.css` that lacks a fingerprint marker, or whose marker
does not parse as `id=<id> fp=<fingerprint>`, MUST be treated as unverifiable
and MUST be discarded at load, independently of every other entry in the
file.

#### Scenario: Entry without a marker is discarded

- GIVEN `colors.css` contains an entry for id 30 with no preceding `/* id=... fp=... */` comment
- WHEN the module loads and parses `colors.css`
- THEN the entry for id 30 is discarded, launcher 30 renders uncoloured, and every
  other well-formed entry is unaffected
- Unit-testable under Meson: yes.

#### Scenario: Entry with a malformed marker is discarded

- GIVEN an entry's marker comment does not match the `id=<id> fp=<fingerprint>` pattern
- WHEN the module loads and parses `colors.css`
- THEN that entry is discarded and every other well-formed entry is unaffected
- Unit-testable under Meson: yes.

### Requirement: Stale Entry Cleanup on Launcher Removal (RF-8)

When a launcher is removed from the panel while the module is loaded, the
system MUST remove its corresponding entry from `colors.css` immediately,
leaving no orphaned rules. Because `xfce4-panel` MAY later recycle a removed
launcher's unique id for an unrelated plugin, and this live cleanup only runs
while the module is loaded, the system MUST additionally reconcile
`colors.css` at every module load using two independent, complementary prune
triggers: (a) the stored id no longer corresponds to a launcher plugin in the
current `xfce4-panel` configuration, and (b) the entry's fingerprint
mismatches per the requirement above. Either trigger alone is sufficient to
prune an entry.
(Previously: cleanup was specified only as a live removal-event action; it
now also runs as load-time reconciliation with two triggers, closing the gap
where a recycled id would otherwise silently inherit an orphaned color when
the module was not loaded at removal time.)

#### Scenario: Live removal while the module is loaded

- GIVEN `colors.css` contains an entry for launcher id 21
- WHEN launcher 21 is removed from the panel while the module is active
- THEN `colors.css` no longer contains an entry for id 21, and every other entry is unchanged
- Unit-testable under Meson: yes, for the cleanup function given a removal event.
  The removal-detection trigger itself needs manual verification against a live panel.

#### Scenario: Load-time pruning when the id is no longer a launcher

- GIVEN `colors.css` contains an entry for id 12, but xfconf now shows plugin id 12
  as a non-launcher plugin type (e.g. a recycled id previously held by a PulseAudio plugin)
- WHEN the module loads and reconciles `colors.css`
- THEN the entry for id 12 is pruned via the id-type trigger, independently of any fingerprint check
- Unit-testable under Meson: yes, for the reconciliation function given a mocked plugin-type lookup.

#### Scenario: Load-time pruning when the id is still a launcher but the fingerprint mismatches

- GIVEN `colors.css` contains an entry for id 13, and xfconf shows plugin id 13 is
  still a launcher, but with a different desktop-file list than the one the stored
  fingerprint was computed from (e.g. a recycled id previously held by a clock plugin,
  now reassigned to an unrelated launcher)
- WHEN the module loads and reconciles `colors.css`
- THEN the entry for id 13 is pruned via the fingerprint-mismatch trigger, even
  though the id-type check alone would not have caught it
- Unit-testable under Meson: yes.

### Requirement: Color Stability Across Moves (RF-9)

The system MUST preserve a launcher's assigned color when the launcher is
moved to a different position or a different panel, because neither its
unique id nor its desktop-file list changes as a result of a move.

#### Scenario: Color follows the launcher after a move

- GIVEN a launcher with id 7 has an assigned color
- WHEN the user moves that launcher to a different position or panel
- THEN the launcher keeps its assigned color, keyed by its unchanged unique id
- Manual verification only: id stability across moves requires a live panel.

#### Scenario: Fingerprint remains valid after a move

- GIVEN a launcher with id 7 has an assigned color and a stored fingerprint
  computed from its desktop-file list
- WHEN the user moves that launcher to a different position or panel, without
  changing its desktop file
- THEN the fingerprint recomputed at the next load still matches the stored
  one, and the color is retained rather than discarded
- Manual verification only for the end-to-end move; the fingerprint
  computation itself is unit-testable in isolation from the move event.

### Requirement: Malformed Configuration Degrades Safely

The system MUST treat a missing, unreadable, or structurally malformed
`colors.css` file as "no colors configured" and MUST NOT propagate a parsing
failure as an error to the user or the panel.

#### Scenario: Malformed file yields no colors, no crash

- GIVEN `colors.css` contains invalid CSS syntax
- WHEN the module loads
- THEN it applies no colors, logs at most one warning, and the panel keeps operating normally
- Unit-testable under Meson: yes, for the parsing/loading function in isolation.
