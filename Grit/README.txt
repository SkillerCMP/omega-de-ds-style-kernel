DS Style 7.4c - Grit image build

Normal kernel build:
  Build-DS-Style-Enhanced.cmd

The normal build performs this image sequence first:
  1. Run the bundled Grit conversion for all 99 compiled image assets.
  2. Compare generated array type, symbol, element count and every image-data value
     against the current known-good .h files.
  3. Stop before replacing anything if any asset differs.
  4. If all assets match, regenerate the root image headers and skin/theme headers.
  5. Continue the normal source validation and kernel build.

Standalone image build:
  Build-All-Image-Files.bat

Intentional artwork change:
  If you deliberately edit a PNG/BMP, the strict comparison will correctly fail
  because the old .h is still the reference. To accept the new artwork once, run:

  powershell -NoLogo -NoProfile -ExecutionPolicy Bypass ^
    -File ".\Grit\Build-All-Image-Files.ps1" -SkipVerify

  That regenerates the .h files from the pictures. A normal build should then
  verify the new picture/header pair and pass.

Files:
  image-build-manifest.json       Root image source/header/symbol/type map.
  Verify-Image-Headers.ps1        Strict Grit reproduction check.
  Build-Root-Image-Files.ps1      Regenerates root image headers.
  Build Skin Files.ps1            Regenerates launcher skin/theme headers.
  Build-All-Image-Files.ps1       Runs the complete graphics pipeline.
