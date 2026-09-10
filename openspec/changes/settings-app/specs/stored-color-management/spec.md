# Stored Colour Management Specification

## Purpose

A GUI view over `colors.css`'s stored entries, joined against a live libwnck
snapshot: only currently open windows are shown, labelled with their real
titles, with single-entry removal and one bulk "remove orphaned entries"
action. The join MUST reuse `lc_winstore_reconcile()`'s fail-safe asymmetry:
an empty or unobtainable snapshot is never read as "no windows are open."

## Requirements

### Requirement: List Shows Only Live Windows, With Real Titles

The list MUST show a row only for a stored entry whose window is currently
open, obtained from a successfully-obtained, non-empty live snapshot, and
MUST label it with that window's current libwnck title. It MUST NOT show a
row, of any kind, for an entry with no matching live window.

#### Scenario: Orphaned entry never appears as a row (headless-testable)

- GIVEN stored entries for window A (currently open) and window B (closed)
- AND a successfully-obtained, non-empty live snapshot containing A but not B
- WHEN the list is built
- THEN it contains exactly one row, for A, and no row exists for B

#### Scenario: Row shows the current title (headless-testable)

- GIVEN a stored entry whose XID matches an open window
- AND a successfully-obtained, non-empty live snapshot
- WHEN the list is built
- THEN the row's label is that window's current title, not a cached or
  stale one

### Requirement: Single Removal of a Listed Entry

The user MUST be able to remove the stored colour for any row currently
listed, leaving every other stored entry untouched.

#### Scenario: Removing one entry leaves the rest intact (headless-testable)

- GIVEN the list shows windows A and C
- WHEN the user removes A's colour
- THEN A's stored entry is deleted and C's stored entry is unchanged

### Requirement: Bulk Orphan Cleanup Is Fail-Safe on an Ambiguous Snapshot

One action MUST report the count of orphaned stored entries (entries with
no matching live window) and remove exactly those entries. This action
MUST treat "the live snapshot could not be obtained" and "the live snapshot
was obtained but is empty" identically — as "cannot determine, prune
nothing" — mirroring `lc_winstore_reconcile()`'s `live_list_obtained ==
FALSE` and `n_live == 0` branches, which return 0 and change nothing in
both cases. It MUST NOT treat either state as evidence that no windows are
open.

#### Scenario: Cleanup removes only confirmed orphans (headless-testable)

- GIVEN 5 stored entries and a successfully-obtained, non-empty live
  snapshot matching 2 of them
- WHEN the user triggers cleanup
- THEN the action reports 3 orphaned entries removed, and the store retains
  only the 2 live entries

#### Scenario: Unobtainable snapshot removes nothing (headless-testable)

- GIVEN stored entries exist
- AND no live snapshot could be obtained (e.g. no `WnckScreen`/display)
- WHEN the cleanup action is evaluated
- THEN it reports 0 orphaned entries and removes nothing

#### Scenario: Empty-but-obtained snapshot removes nothing (headless-testable)

- GIVEN stored entries exist
- AND the live snapshot was successfully obtained but currently reports
  zero windows (the libwnck asynchronous-population case that previously
  caused real data loss)
- WHEN the cleanup action is evaluated
- THEN it reports 0 orphaned entries and removes nothing, exactly as in the
  unobtainable-snapshot case

#### Scenario: List never marks entries as orphaned under an ambiguous snapshot (headless-testable)

- GIVEN stored entries exist
- AND the live snapshot is either unobtainable or obtained-but-empty
- WHEN the list view is built
- THEN no stored entry is presented as orphaned, and the list shows no rows
  as live either — it MUST NOT render every stored entry as if orphaned

#### Scenario: Cleanup is idempotent on a stable, valid snapshot (headless-testable)

- GIVEN a successfully-obtained, non-empty live snapshot with no window
  closing between two consecutive cleanup invocations
- WHEN cleanup runs a second time immediately after the first
- THEN the second run reports 0 orphaned entries removed
