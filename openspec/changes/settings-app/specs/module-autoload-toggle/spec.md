# Module Autoload Toggle Specification

## Purpose

Pure, headless-testable enable/disable logic for `/Gtk/Modules` — list
transforms over `gchar**` (`lc_autoload_add`/`remove`/`is_enabled`) isolated
from xfconf I/O behind a two-function `LcAutoloadBackend` vtable (decision
D13). The property has no fixed schema type: it may be absent, a scalar
string (as the README's own documented command creates today), or an array,
and may already hold other applications' modules. The panel restart stays a
separate, explicit, cancellable action, never automatic.

## Requirements

### Requirement: Pure List Transforms

`lc_autoload_add`, `lc_autoload_remove`, and `lc_autoload_is_enabled` MUST
operate only on an in-memory `gchar**` list with no I/O of their own, and
MUST be testable against a fake `LcAutoloadBackend`.

#### Scenario: Add to an empty list (headless-testable)

- GIVEN an empty module list
- WHEN `lc_autoload_add` is called with our module name
- THEN the resulting list contains exactly our module

#### Scenario: Remove preserves other entries (headless-testable)

- GIVEN a list containing our module and at least one other module
- WHEN `lc_autoload_remove` is called
- THEN our module is removed and every other entry survives, unchanged, in
  its original order

#### Scenario: Remove when absent is a no-op (headless-testable)

- GIVEN a list that does not contain our module
- WHEN `lc_autoload_remove` is called
- THEN the list is unchanged and no removal is reported

### Requirement: `/Gtk/Modules` Shape Handling

Enabling and disabling MUST correctly detect and handle the key being
absent, a scalar string, or an array, and MUST never drop or alter any
entry that is not our module, regardless of shape. Enabling MUST be
idempotent; disabling when we are not present MUST be a no-op.

#### Scenario: Key absent, enable (headless-testable)

- GIVEN `/Gtk/Modules` is absent
- WHEN the module is enabled
- THEN the key is created holding exactly our module

#### Scenario: Key is a scalar holding only our module, enable is idempotent (headless-testable)

- GIVEN `/Gtk/Modules` is a scalar string equal to our module name
- WHEN the module is enabled again
- THEN the result still holds exactly our module, with no duplication

#### Scenario: Key is a scalar holding a different module, enable preserves it (headless-testable)

- GIVEN `/Gtk/Modules` is a scalar string holding a different module's name
- WHEN our module is enabled
- THEN the result holds both that other module's entry, byte-for-byte
  unchanged, and our module
- AND no entry is duplicated

#### Scenario: Key is an array with third-party modules, enable preserves them (headless-testable)

- GIVEN `/Gtk/Modules` is an array holding several third-party modules
- WHEN our module is enabled
- THEN our module is added, every third-party entry survives unchanged,
  and our module appears exactly once

#### Scenario: Enabling twice is idempotent (headless-testable)

- GIVEN our module is already present, in any shape
- WHEN enable is invoked a second time
- THEN the stored value is unchanged from after the first enable — no
  duplicate entry is created

#### Scenario: Key is a scalar holding only our module, disable clears it (headless-testable)

- GIVEN `/Gtk/Modules` is a scalar string equal to our module name
- WHEN our module is disabled
- THEN the key ends absent (matching the README's documented reset command)

#### Scenario: Key is an array with third-party modules, disable removes only ours (headless-testable)

- GIVEN `/Gtk/Modules` is an array holding third-party modules and ours
- WHEN our module is disabled
- THEN only our module is removed and every third-party entry survives,
  byte-for-byte unchanged

#### Scenario: Disabling when we are not present is a no-op (headless-testable)

- GIVEN `/Gtk/Modules` does not contain our module, in any shape
- WHEN disable is invoked
- THEN the stored value is unchanged and no write occurs

### Requirement: Restart Stays Separate, Explicit, and Cancellable

The system MUST NOT restart `xfce4-panel` automatically as a side effect of
an enable or disable write. Restarting MUST be a distinct, user-triggered
action that can be cancelled without discarding the xfconf change already
made.

#### Scenario: Toggle completes without restarting (manual)

- GIVEN the user confirms an enable or disable action
- WHEN the xfconf write completes
- THEN the panel is not restarted, and a separate "restart panel" action is
  offered

#### Scenario: Restart is cancellable (manual)

- GIVEN the restart action is offered after a toggle
- WHEN the user cancels it
- THEN no restart occurs and the xfconf change persists as written

#### Scenario: Confirmed restart applies the change (manual)

- GIVEN the user explicitly confirms the restart action
- WHEN it runs
- THEN `xfce4-panel` restarts and the new module state takes effect
