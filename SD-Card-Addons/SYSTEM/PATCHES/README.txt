DS Style - External GBA Auto Patches v2

Copy the PATCHES folder to /SYSTEM on the SD card.

Layout:
  /SYSTEM/PATCHES/GBA/<first game-code char>/<second game-code char>/<GAME>.patch

FORMAT=2 keeps the original reset/IRQ variants and also externalizes the active
game-specific compatibility data that used to be hard-coded in GBApatch.c.

Commands:
  IRQ32=offset
      Deferred IRQ relocation. The loader validates the original word is
      03007FFC or 03FFFFFC, then adds replacement 03007FF4.
  NO_IRQ=1
      Variant intentionally has no reset/IRQ relocation offsets.
  ADD32=offset,value
      Deferred 32-bit record passed through Add2()/iPatchInfo2, preserving the
      old Patch_SpecialROM_sleepmode behavior.
  PATCH16=offset,value
  PATCH32=offset,value
      Direct boot-time ROM writes. PATCH16 holds the former Dragon Ball/Top Gun
      compatibility edits.
  TRIM=size
      Exact legacy iTrimSize override.
  SEARCH32PAIR=start,length,word0,word1,write_delta,replacement
      Bounded legacy pair search. Currently used for 2GBP only.

The legacy Fire Emblem Check_Fire_Emblem() research was already disabled in the
kernel. It is preserved only as # commented lines in the relevant .patch files
and is not parsed or executed.

The compiled reset table and game-specific switch statements are no longer the
runtime source of compatibility data. If a v2 file is absent or has no safe IRQ
variant, the generic ROM IRQ scanner remains the safety fallback.

Compatibility note: iPatchInfo2 still has EMax=32. Two legacy reset rows contain
more than 32 IRQ offsets; v2 intentionally validates/applies the same first 32
records, preserving inherited behavior.
