# v0.1.0 — Colour your panel's window buttons

First release. Right-click any window button in the XFCE panel, pick a colour, and that button gets painted. Four windows of the same application stop looking like four identical icons.

## Install

```sh
sudo dpkg -i xfce4-window-button-colors_0.1.0_amd64.deb
sudo apt-get -f install
```

Then enable it for your session and restart the panel:

```sh
xfconf-query -c xsettings -p /Gtk/Modules -n -t string -s xfce4-window-button-colors
xfce4-panel -r
```

Installing does not enable it. Enabling writes to your session's GTK module list, which persists across logins, so it is left as your explicit and reversible decision rather than something an install step does on your behalf.

Verify the `.deb` against `SHA256SUMS` below if you like.

## What you get

| | |
|---|---|
| Quick palette | Eight swatches directly in the right-click menu |
| Full picker | Wheel, sliders, hex and transparency, behind **Custom…** |
| Eyedropper | Take a colour from anywhere on screen |
| Clear | **No colour**, reachable from every place a colour can be set |
| Current state | A tick marks whichever entry is applied |

## Know before you install

**Colours are per window and survive a panel restart, not an application restart.** A colour is bound to the X window id, the only thing that tells one window of an application apart from another. Reopening an application creates a new window with a new id, so its colour is gone.

Binding to the application instead would persist forever but paint every window of that app identically — the exact problem this tool exists to solve. The window title is not usable as an anchor either, because it changes as you work.

**The module does nothing outside `xfce4-panel`.** It is loaded in-process, so it checks which program it is in before allocating anything, and returns immediately everywhere else. Verified against the panel's own plugin wrapper processes, which inherit the module and are correctly ignored.

**It will not take your panel down.** A missing, unreadable or malformed configuration degrades to "no colours" with a single warning. There is no abort path in the loading code.

## Requirements

`xfce4-panel` 4.16+, GTK 3.22+, GLib 2.56+, libwnck 3. Built and tested against XFCE 4.18.4 on Ubuntu 24.04.

## Notes for the curious

The colour picker's alpha is formatted locale-independently. Under any comma-decimal locale — Spanish, German, French — the obvious `printf("%.2f")` emits `0,85`, GTK rejects the entire CSS rule, and every colour silently fails to paint with nothing but a log line to show for it. The project's test suite runs under `es_ES.UTF-8` in CI specifically so this cannot come back.

Colours are stored in `~/.config/xfce4-window-button-colors/`. The file is generated; a hand-edited or truncated one degrades gracefully rather than being discarded.

## Not in this release

Rules of the shape *"windows of app X whose title contains Y always get colour Z"*, which would make a colour outlive an application restart. Two windows with identical titles are a known hard case for identifying which button belongs to which window.

## Licence

GPL-2.0-or-later.
