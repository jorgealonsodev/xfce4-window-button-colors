# v0.2.0 — Turn it on without a terminal

v0.1.0 shipped a module you had to enable with `xfconf-query`. This release adds a settings application in the XFCE Settings Manager: an enable switch, a separate panel restart, and a list of your coloured windows.

## Install

```sh
sudo dpkg -i xfce4-window-button-colors_0.2.0_amd64.deb
sudo apt-get -f install
```

Then open **Settings → Window Button Colours** and turn it on. No terminal.

Installing still does not enable the module on its own. Enabling writes to your session's GTK module list, which persists across logins, so it stays your explicit and reversible decision — now made with a switch instead of a command.

## What you get

| | |
|---|---|
| Enable switch | Turns the module on and off, from the menu |
| Restart panel | A separate action, always confirmed, never automatic |
| Colour list | Your coloured windows, with their live titles |
| Remove | One entry, or all the ones whose windows are gone |

## A correction to v0.1.0's documentation

The old README told you to disable with `xfconf-query -c xsettings -p /Gtk/Modules -r`. **That command resets the whole key**, so if another GTK module shared it — a common thing — you removed that module's autoload too, and would only notice at your next login.

The ordinary way to disable is now the switch. The reset command survives only as the emergency recovery from a TTY, where clearing the key is exactly what you want.

## Why the toggle never restarts the panel for you

Enabling writes a session-wide key. If a module there ever failed to load you would be left without a panel at the next login, recoverable only from a TTY. Restarting is therefore a second, deliberate action with its own confirmation, and that confirmation tells you the recovery command before you take the risk — not afterwards in a document.

## Colours whose window has closed

Your colours are bound to the X window id, which is what tells one window of an application apart from another. When a window closes, its id is gone and no name can be recovered for it, so those entries are not listed as rows. Instead the app offers to remove them in one action and tells you how many there are.

If the window list cannot be read reliably, the app says so and **disables the cleanup** rather than showing an empty list. An empty list reads as "you have no colours", and acting on that would delete colours you still want.

## Under the hood, for the curious

Every rule that decides anything lives in a pure core with no GTK, xfconf or libwnck in it, and is unit-tested against fake backends — including all six shapes the `/Gtk/Modules` key can take. Fifteen of those tests assert that a **third-party module's entry survives byte-for-byte**, because the real risk of this feature was never breaking itself; it was quietly breaking somebody else's.

CI runs the whole suite a second time under a comma-decimal locale, because a locale-dependent formatting bug once made every colour fail to paint with nothing but a log line to show for it.

## Known limitations

- The new binary has no man page yet.
- Colours still do not survive restarting an application, for the reason above. Rules of the shape "windows of app X whose title contains Y always get colour Z" would fix that and are not in this release.

## Requirements

`xfce4-panel` 4.16+, GTK 3.22+, GLib 2.56+, libwnck 3, xfconf 4.12+. Built and tested against XFCE 4.18.4 on Ubuntu 24.04.

## Licence

GPL-2.0-or-later.
