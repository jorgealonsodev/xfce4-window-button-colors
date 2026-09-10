# Manual verification — settings application

Written 2026-09-10, for the `settings-app` change (`openspec/changes/settings-app/`).

These are the scenarios that offline verification (headless unit tests, `meson test`,
an Xvfb run without a full desktop session) genuinely cannot cover: a real xfconf
daemon, a real XFCE Settings Manager, a real window manager reporting real titles,
and human judgement of a dialog's wording. Each step names the exact action and the
exact expected result — follow it in order on a real XFCE 4.18 session with
`xfce4-panel` and `xfsettingsd` running.

Do **not** run this checklist against your only working session without a way back
to a TTY — step 5 deliberately produces a state where the panel may not come back on
its own, and the whole point of this checklist is to confirm the recovery path
actually works.

Prerequisite: build and install the package (see the README's
[Building from source](../README.md#building-from-source) or
[Quick path](../README.md#quick-path)), or run
`build/src/settings/xfce4-window-button-colors-settings` from a source build.

## 1. Settings Manager entry and standalone launch

Covers `settings-app-shell`'s "Entry visible after install", "Launch while module is
disabled", and "Launch while module is enabled" scenarios.

- [ ] **1.1 Entry visible.** With the package installed, open the XFCE Settings
  Manager. **Expect**: an entry named "Window Button Colors" is listed, with an icon
  and category consistent with XFCE's own settings dialogs (grouped alongside other
  XFCE settings, not under a generic "Other" section).
- [ ] **1.2 Launch while disabled.** Confirm `xfce4-window-button-colors` is absent
  from `/Gtk/Modules` (`xfconf-query -c xsettings -p /Gtk/Modules`), then launch the
  entry from step 1.1. **Expect**: the window opens and is usable — the toggle
  and restart button are both present and responsive.
- [ ] **1.3 Launch while enabled.** With `xfce4-panel` running and the module already
  enabled (from an earlier run of this checklist, or via the toggle below), launch
  the settings application again. **Expect**: the window opens without requiring a
  panel restart, and reflects the currently-enabled state (see 2.1).

## 2. Toggle: xfconf write and reflection

Covers `module-autoload-toggle`'s "Toggle completes without restarting" and D5's
external-edit convergence.

- [ ] **2.1 Toggle on writes xfconf, does not restart.** With the module disabled,
  open the settings application and turn **Enable window button colours** on.
  **Expect**: `xfconf-query -c xsettings -p /Gtk/Modules` now includes
  `xfce4-window-button-colors` (as a scalar if the key was absent, or added to
  whatever shape was already there). The panel does **not** restart on its own — the
  "configuration changed" hint appears, but the panel button bar is untouched until
  you click Restart.
- [ ] **2.2 Toggle off writes xfconf, does not restart.** Turn the switch off.
  **Expect**: `xfce4-window-button-colors` is removed from `/Gtk/Modules` (or, if the
  key held other modules too, only this module's entry is gone — confirm any other
  entry you added for 3.3 is still present, byte-for-byte). Again, no restart happens
  on its own.
- [ ] **2.3 External edit reflects into the switch.** With the settings window still
  open, run `xfconf-query -c xsettings -p /Gtk/Modules -n -t string -s
  xfce4-window-button-colors` from a terminal (simulating a concurrent edit — by
  another tool, or a second instance of this application). **Expect**: the switch in
  the already-open window flips to reflect the new state without you touching it, and
  the "configuration changed" hint appears. Repeat with the external edit turning it
  back off; the switch flips back.
- [ ] **2.4 Second instance converges too.** With the module enabled, open a second
  copy of the settings application. **Expect**: its switch starts in the correct
  (on) position. Toggle it off in the second window; **expect** the first window's
  switch flips off within the same session (2.3's mechanism, not a fresh read).

## 3. Restart action

Covers `module-autoload-toggle`'s "Restart is cancellable" and "Confirmed restart
applies the change", and D4's subprocess contract.

- [ ] **3.1 Cancel leaves the xfconf change in place.** Toggle the switch (either
  direction), then click **Restart Panel**, then click **Cancel** in the confirmation
  dialog. **Expect**: no restart happens — the panel keeps running uninterrupted —
  and the xfconf value from the toggle is still exactly what you set (recheck with
  `xfconf-query`).
- [ ] **3.2 Confirmed restart actually restarts the panel.** Toggle the switch on,
  click **Restart Panel**, then click **_Restart** in the confirmation dialog.
  **Expect**: the panel briefly disappears and reappears (a real `xfce4-panel -r`),
  and the module's effect now applies — right-clicking a window button shows the
  **Background colour** menu entry.
- [ ] **3.3 Third-party survival end-to-end.** Before 3.2, add a second, real,
  harmless GTK module to `/Gtk/Modules` by hand, e.g.
  `xfconf-query -c xsettings -p /Gtk/Modules -n -t string -s
  canberra-gtk-module:xfce4-window-button-colors`. Toggle this module off and back
  on through the settings application (steps 2.1/2.2), restarting each time.
  **Expect**: `canberra-gtk-module`'s entry survives every toggle, byte-for-byte,
  and (if it was working before) its own behaviour — event sounds — is unaffected
  after the panel restarts.
- [ ] **3.4 Recovery from a failed restart.** If a restart ever leaves the panel not
  coming back (only likely if something else on the system is broken — this is a
  recovery-path check, not an expected outcome), switch to a TTY (Ctrl+Alt+F3 or
  similar) and run the exact command the confirmation dialog and the README's
  Troubleshooting table both name:
  `xfconf-query -c xsettings -p /Gtk/Modules -r`, then log back in. **Expect**: the
  panel comes back on the next login, with the module removed from
  `/Gtk/Modules` (this resets the whole key — acceptable here because it is the
  emergency path, not the ordinary one; see README's
  [Enabling and disabling](../README.md#enabling-and-disabling)).

## 4. Spawn-failure error surface

Covers D4's non-modal error surfacing.

- [ ] **4.1 Simulate a spawn failure.** Temporarily hide `xfce4-panel` from `PATH`
  for the settings application's process only, e.g. launch it as
  `env PATH=/usr/bin:/bin xfce4-window-button-colors-settings` from a shell where you
  have first confirmed `xfce4-panel` is *not* in `/usr/bin` or `/bin` (check with
  `which xfce4-panel` under that restricted `PATH`; adjust the override to whatever
  strips it on your system). Click **Restart Panel** → **_Restart**.
  **Expect**: a non-modal error dialog appears (you can still interact with the main
  settings window while it is open) showing the failure message together with the
  literal failed command (`xfce4-panel -r`) in a selectable label you can copy. No
  automatic retry happens. The main window and its toggle remain fully usable
  afterward.

## 5. Stored-colour list — WITHDRAWN

**Note (2026-09-10): the stored-colour management section (the row list, per-row
removal, and "Clean Up Orphaned Entries") has been removed from the settings
application.** Reason: "Clean Up Orphaned Entries" caused real, reproduced data
loss on the maintainer's own configuration (colours deleted for windows that were
still open), and the failure was not reliably reproducible from the same fixture
and single click, so it could not be pinned down and fixed in place. This checklist
no longer instructs a tester to verify that section — the scenarios that used to
live here (`stored-color-management`'s row/title/removal/cleanup scenarios) do not
apply until the feature returns.

## 6. Locale

- [ ] **6.1 Spanish strings render.** Launch the settings application under
  `LC_ALL=es_ES.UTF-8 xfce4-window-button-colors-settings` (installing the `es_ES`
  locale first if needed: `sudo locale-gen es_ES.UTF-8`). **Expect**: every string
  from sections 1-4 above (toggle label, hint, restart dialog and its secondary
  text, error dialogs) appears in Spanish, with no untranslated English string and
  no `msgid` literal shown in the UI.

## 7. Packaging

Covers `settings-app-shell`'s "Package is lintian-clean" (also re-run by
`tasks.md` 8.6's automated step; listed here for the case where you are working
through this checklist without also running the full verification command list).

- [ ] **7.1 `lintian` is clean.** `dpkg-buildpackage -us -uc -b` from the repository
  root, then `lintian ../xfce4-window-button-colors_0.2.0_amd64.changes` (or the
  `.deb` directly). **Expect**: no errors; any informational tags are pre-existing
  and unrelated to this change.
- [ ] **7.2 The `.deb` contains everything.** `dpkg -c
  ../xfce4-window-button-colors_0.2.0_amd64.deb | grep -E
  'xfce4-window-button-colors-settings$|\.desktop$|es\.mo$'`. **Expect**: three
  matches — the settings binary, the installed `.desktop` file, and the compiled
  Spanish catalogue.
