# DS Style 7.4c

Source release for **DS Style 7.4c** for the EZ-FLASH OMEGA Definitive Edition.

The launcher displays **DS Style 7.4c** on screen. GBA savestates use the RTS3 hardware-checkpoint core.

## Build

On Windows with the existing devkitARM toolchain installed, run:

```text
Build-DS-Style-Enhanced.cmd
```

The build helper first verifies the editable PNG/BMP graphics by regenerating all 99 compiled image assets with the bundled Grit tool and comparing their image-array data with the known-good headers. If every asset matches, it regenerates the image headers, performs source validation and the native RTS assembly preflight, then builds the kernel. A successful native build produces `ezkernelnew.bin`.

The image verification is intentionally strict: a changed picture that no longer reproduces the current compiled asset stops the build before any header is replaced. `Grit\Build Skin Files.bat` runs the same complete graphics pipeline by itself.

Build support files:

- `Build-DS-Style-Enhanced.ps1`
- `Build-RTS-Assembly-Preflight.ps1`
- `Makefile`

## SD card patches

All packaged GBA compatibility patch profiles are contained under:

```text
SD-Card-Addons/SYSTEM/PATCHES/GBA/
```

The files directly under `SD-Card-Addons/SYSTEM/PATCHES/` document and verify that patch database. Copy the `SD-Card-Addons/SYSTEM` folder to the root of the SD card and merge it with the existing `SYSTEM` folder. At runtime the kernel still loads these profiles from `/SYSTEM/PATCHES/GBA/`.

`SD-Card-Addons/` also contains the optional companion text/Markdown plug-ins.

## Savestates

GBA savestates use RTS3. The source includes the current RTS3 inspector and maintenance tools under `tools/`.

```text
python tools/inspect_rts3.py "Game.rts"
```

## Documentation

The user guide is:

```text
docs/DS Style User Guide.pdf
```

## License

See `LICENSE` and the licenses supplied with third-party components in their respective directories.
