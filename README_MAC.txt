calcyx for macOS
================

This disk image contains the calcyx GUI (calcyx.app) plus the integrated
CLI / TUI binary embedded inside the bundle.


Installation
------------

1. Drag calcyx.app into the Applications folder (the symlink in this
   disk image points there).

2. (Optional) Expose the CLI / TUI on your shell PATH:

       sudo ln -sfn /Applications/calcyx.app/Contents/MacOS/calcyx-cli \
                    /usr/local/bin/calcyx

   After this you can run `calcyx -e '3+5'` from any terminal, and
   `calcyx file.txt` to launch the TUI editor on a sheet file.


Alternative: install via Homebrew
---------------------------------

If you already use Homebrew, the recommended path is:

       brew tap ponzu840w/calcyx
       brew install calcyx

This installs both the CLI (`calcyx`, on PATH automatically) and the GUI
bundle, and `brew upgrade` keeps everything up to date.


Why is the CLI inside the .app bundle?
--------------------------------------

macOS conventions favour a self-contained .app that you simply drop into
/Applications. We follow that for the GUI. The CLI is shipped inside the
bundle (Contents/MacOS/calcyx-cli) so that the disk image stays a single
.app and yet command-line users can still extract it with the symlink
above. Manual pages (`man calcyx`) are only available through the
Homebrew formula because the macOS man system does not look inside .app
bundles.


License
-------

calcyx is distributed under the MIT licence. See LICENSE on this disk
image, and THIRD_PARTY_LICENSES.txt for upstream credits (FLTK, FTXUI,
mpdecimal, ...).
