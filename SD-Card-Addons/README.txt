DS Style 7.4c - SD Card Add-ons
================================

Copy the SYSTEM folder in this directory to the root of the SD card and merge
it with the existing SYSTEM folder.

GBA compatibility patch database:

  SYSTEM/PATCHES/GBA/...

The kernel loads these profiles at runtime from /SYSTEM/PATCHES/GBA/.

Optional Text / Markdown viewer plug-ins:

  SYSTEM/PLUG/txt.bin
  SYSTEM/PLUG/md.bin
  SYSTEM/PLUG/markdown.bin

The launcher resolves these plug-ins by file extension. No additional kernel
source changes are required for .txt, .md or .markdown files.
