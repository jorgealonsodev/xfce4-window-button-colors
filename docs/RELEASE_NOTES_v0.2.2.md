# v0.2.2 — The stored-colour list is withdrawn

The settings window's colour list and its **Clean Up Orphaned Entries** button are removed. The enable switch and **Restart Panel** stay and are unaffected.

## Install

```sh
sudo dpkg -i xfce4-window-button-colors_0.2.2_amd64.deb
```

**If you are on v0.2.0 or v0.2.1, update.** Those versions can delete colours you still want.

## Why it was removed rather than fixed

**Clean Up Orphaned Entries could delete colours whose windows were still open.** Reproduced on a real configuration: three stored colours, one window genuinely closed, one click — all three gone.

The rule that decides which colours are stale is correct. Tested in isolation with one dead window and two live ones, it drops exactly one. The defect is somewhere in the settings window's own code, and it is **not reliably reproducible**: the same file and the same single click produced a full wipe once and correct behaviour the next time.

A button that sometimes destroys your work is worse than no button. It stays out until the cause is known, not until it looks fine.

## What you lose

Nothing that worked. The list only ever showed windows currently open, and removing a colour has always been possible where you set it: right-click the window button, then **No colour**.

Stale entries for closed windows accumulate in `~/.config/xfce4-window-button-colors/colors.css` with no way to clear them from the UI. They are inert — they paint nothing — and the file stays small. You can delete them by hand if you want to.

## What was kept

The logic behind the list is correct and fully tested, so it stays in the codebase rather than being deleted along with the button. When the feature returns, the part that was never at fault does not have to be written again.

## Also in this release

The settings application no longer links libwnck at all, since the list was its only user. The panel module still does, and still needs it.

## Licence

GPL-2.0-or-later.
