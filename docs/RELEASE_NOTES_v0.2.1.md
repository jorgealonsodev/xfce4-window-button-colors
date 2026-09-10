# v0.2.1 — The stored colour list now appears

v0.2.0's settings window always said *"the window list is currently unavailable"* and kept **Clean Up Orphaned Entries** greyed out, even on a healthy desktop with windows open. This release fixes that. The list now shows your coloured windows by their current titles, with a Remove button on each.

## Install

```sh
sudo dpkg -i xfce4-window-button-colors_0.2.1_amd64.deb
```

Nothing else changed. If v0.2.0 works for you and you never open the colour list, you can skip this.

## What was wrong

The window shows its list once, when it appears. It asked the window manager for the list of open windows from inside GTK's `map` handler — and **being mapped is not the same as being known to the window manager**. At that instant GTK has mapped the window but the WM has not processed it yet, so the settings window itself was missing from the list.

That missing entry is exactly the signal the app uses to detect an incomplete answer, so it concluded — correctly — that it could not trust what it had been told, said so, and disabled the cleanup action. Nothing retried afterwards, so it stayed that way for the life of the window.

The probe now runs one main-loop iteration later, which is enough for the window manager to catch up.

## Why it was not caught earlier

Offline testing under a virtual display could reach the cautious branch but never the successful one. The failure needed a real desktop with a real window manager, which is exactly what the project's own manual checklist says about this path — the checklist was right, and the bug was sitting on the far side of it.

The safety behaviour itself was never wrong. Faced with an answer it could not verify, the app refused to present a list rather than showing an empty one and offering to delete from it. That is the behaviour that protects your colours, and it is unchanged.

## Licence

GPL-2.0-or-later.
