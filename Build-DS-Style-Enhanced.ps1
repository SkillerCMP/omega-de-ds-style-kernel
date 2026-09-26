param([string]$Root = $PSScriptRoot)
$ErrorActionPreference = 'Stop'

function Read-All([string]$Relative) {
    $Path = Join-Path $Root $Relative
    if (-not (Test-Path -LiteralPath $Path)) { throw "Required file is missing: $Relative" }
    return [System.IO.File]::ReadAllText($Path)
}
function Need([string]$Relative, [string]$Needle, [string]$Label) {
    $Text = Read-All $Relative
    if ($Text.IndexOf($Needle, [System.StringComparison]::Ordinal) -lt 0) {
        throw "$Label is missing from $Relative"
    }
}
function Reject([string]$Relative, [string]$Needle, [string]$Label) {
    $Text = Read-All $Relative
    if ($Text.IndexOf($Needle, [System.StringComparison]::Ordinal) -ge 0) {
        throw "$Label is still present in $Relative"
    }
}

# DS Style 7.4c structural source checks.
# This validates the drop-in optimized source only.
# Grit image regeneration/verification is performed by Build-DS-Style-Enhanced.cmd
# before this structural source validation. This script does not rerun Grit or the developer test suite.
Need 'source\launcher_version.h' '#define LAUNCHER_VERSION_TEXT "7.4c"' 'DS Style 7.4c version marker'
Need 'source\ezkernelnew.c' 'Launcher_RedrawRomMenuSaveTypeValue' 'v7.3 save-type redraw'
Need 'source\ezkernelnew.c' 'Launcher_RestoreHorizontalOuterBorder' 'v7.3 carousel border fix'
Need 'source\ezkernelnew.c' 'old_view == LAUNCHER_VIEW_LIST && new_view == LAUNCHER_VIEW_LIST_ART' 'v7.3 List + Art transition'
Need 'source\ezkernelnew.c' 'if(!top && (base == launcher_current_theme_bg))' 'v7.3 full-screen-theme path'
Need 'source\showcht.c' "Avoid clearing the whole body to preserve v7.3's flicker-free redraw." 'merged v7.3 cheat redraw'

Need 'source\showcht.h' '#define MAX_RUNTIME_CHEAT_RECORDS 128' 'Enhanced 128-record limit'
Need 'source\showcht.h' 'CHT_OP_IF_EQ' 'Enhanced condition opcodes'
Need 'source\showcht.h' 'CHT_OP_WRITE16' 'Enhanced W16 opcode'
Need 'source\showcht.h' 'CHT_OP_WRITE32' 'Enhanced W32 opcode'
Need 'source\showcht.h' 'CHT_OP_PTR' 'Enhanced PTR opcode'
Need 'source\showcht.h' 'CHT_ROM_GROUP' 'Enhanced ROM groups'
Need 'source\showcht.c' 'typedef enum CHT_COMMAND_' 'command enum parser'
Need 'source\showcht.c' 'DecodeCommandKey' 'one-pass command decoder'
Need 'source\showcht.c' 'GetVisibleCheatMenuEntryIndex' 'collapsible group mapping'
Need 'source\showcht.c' 'SetCheatGroupExpanded' 'collapsible group state'
Need 'source\showcht.c' 'static char *const cheat_line_buf = (char*)(pReadCache + CHT_LINE_BUFFER_OFFSET);' 'shared-cache cheat line buffer'
Reject 'source\showcht.c' 'char buf[MAX_BUF_LEN + 1]EWRAM_BSS;' 'old permanent cheat line buffer'
Reject 'source\showcht.c' 'char _paramv[MAX_BUF_LEN]' 'duplicate parser buffer'

Need 'source\gba_rts_patch.s' '.rodata' 'ROM-resident injected runtime template'
Need 'source\gba_rts_patch.s' 'Enhanced runtime opcode dispatch' 'optimized runtime dispatcher'
Need 'source\gba_rts_patch.s' 'cheat_direct_write:' 'direct W8 fast path'
Need 'source\gba_rts_patch.s' 'cheat_condition_stack:' 'condition stack'
Need 'source\gba_rts_patch.s' '.space 0x10' 'packed 16-level condition stack'
Need 'source\gba_rts_patch.s' 'cheat_read_data_record:' 'shared FC reader'
Need 'source\gba_rts_patch.s' 'cheat_write_width:' 'shared W16/W32 handler'
Need 'source\gba_rts_patch.s' 'cheat_if_true:' 'optimized IF comparator'
Reject 'source\gba_rts_patch.s' '.space 0x400' 'obsolete blank cheat table'
Reject 'source\gba_rts_patch.s' '.space 0x80' 'obsolete unpacked condition stack'
Reject 'source\gba_rts_patch.s' 'cheat_write16:' 'obsolete separate W16 handler'
Reject 'source\gba_rts_patch.s' 'cheat_write32:' 'obsolete separate W32 handler'

Need 'source\ff15\ffconf.h' 'FF_PRINT_LLI' 'FatFs formatter configuration'
Need 'source\ff15\ffconf.h' 'FF_PRINT_FLOAT' 'FatFs formatter configuration'
Need 'source\ezkernelnew.c' '#define LAUNCHER_LIST_ART_CACHE_COUNT 2' 'two-slot List + Art cache'
Need 'source\ezkernelnew.c' '#define launcher_start_preview_cache launcher_thumbnail_workspace.start_preview' 'shared Start preview workspace'
Need 'source\ezkernelnew.c' 'launcher_custom_thumb_manifest_hash[LAUNCHER_CUSTOM_THUMB_MANIFEST_MAX]' 'single active manifest table'
Need 'source\ezkernelnew.c' 'Launcher_DrawThemeTopbarClip' 'compact top-bar renderer'
Need 'source\launcher_theme_assets.h' 'launcher_topbar_patterns.h' 'compact top-bar theme table'
Need 'source\launcher_topbar_patterns.h' 'LAUNCHER_TOPBAR_PATTERN_COUNT' 'compact top-bar patterns'

Need 'source\GBApatch.c' 'SetRtsStateIdentity' 'save-state game identity'
Need 'source\GBApatch.c' 'PatchRtsStateIdentity' 'save-state identity injection'
Need 'source\gba_rts_patch.s' 'S_RTS_INVALID' 'combined state invalid marker'
Need 'source\gba_rts_patch.s' 'S_RTS_FLAG' 'combined state valid marker'
Need 'source\gba_rts_only.s' 'S_RTS_INVALID' 'RTS-only invalid marker'
Need 'source\gba_rts_only.s' 'S_RTS_FLAG' 'RTS-only valid marker'
Need 'source\ezkernelnew.c' 'f_size(&file) != 0x70000' 'exact RTS file-size validation'
Need 'source\ezkernelnew.c' 'Check_game_RTS_FAT' 'RTS FAT extent validation'

Need 'source\ezkernelnew.c' 'g_ui_audio_buffer[5504]' '13.2 right-sized audio buffer'
Need 'source\ezkernelnew.c' '#define UI_AUDIO_BUFFER_SIZE 5504' '13.2 audio buffer size constant'
Reject 'source\ezkernelnew.c' 'g_ui_audio_buffer[0x2000]' 'old 8192-byte audio buffer'
Need 'source\ezkernelnew.c' 'static const u8 launcher_scale84_x[84]' '13.2 ROM scale maps'
Need 'source\ezkernelnew.c' '#define launcher_scale80_56 launcher_scale56_y' 'shared 56-entry scale map'
Need 'source\ezkernelnew.c' '#define launcher_scale80_32 launcher_scale32_y' 'shared 32-entry scale map'
Reject 'source\ezkernelnew.c' 'Launcher_InitScaleMaps' 'runtime scale-map generator'
Need 'source\ezkernelnew.c' '#define LAUNCHER_LIST_ART_SPAN_ROWS 62' '13.2 relative span row count'
Need 'source\ezkernelnew.c' 'Launcher_ListArtSpanCacheGet' 'relative span cache accessor'
Reject 'source\ezkernelnew.c' 'launcher_list_art_span_count[160]' 'old full-screen span cache'

Need 'source\saveMODE.h' '#define SAVE_MODE_PACKED_ENTRY_COUNT 2768u' 'packed save-mode count'
Need 'source\saveMODE.h' 'save_mode_packed_table[]' 'packed save-mode data'
Reject 'source\GBApatch.c' '#include "reset_table.h"' 'compiled reset compatibility table include'
Reject 'source\GBApatch.c' 'reset_table_packed' 'compiled reset compatibility data'
Reject 'source\GBApatch.c' 'use_internal_engine' 'legacy internal reset lookup'
Need 'source\ezkernelnew.c' 'SaveMode_EncodeGameCode' 'packed save-mode encoder'
Need 'source\ezkernelnew.c' 'SAVE_MODE_PACKED_ENTRY_COUNT' 'save-mode binary search'
Reject 'source\ezkernelnew.c' 'dmaCopy((void*)saveMODE_table' 'old save-mode cache copy'
Reject 'source\ezkernelnew.c' 'for(i=0;i<3000;i++)' 'old hardcoded save-mode scan'

# Retain the key 13.4 safety fixes.
Need 'source\ezkernelnew.c' '#define LAUNCHER_MAX_RECENTS 10' 'bounded recent-entry count'
Need 'source\ezkernelnew.c' 'Recent_GetLoadedPathAt' 'bounded recent path parser'
Need 'source\ezkernelnew.c' 'FA_WRITE | FA_CREATE_ALWAYS' 'truncating recent file rewrite'
Need 'source\ezkernelnew.c' 'Launcher_SortCustomThumbManifest();' 'sorted custom thumbnail manifest'
Need 'source\ezkernelnew.c' 'u16 p_folder_select_show_offset' 'narrow folder offset history'
Need 'source\ezkernelnew.c' 'u8 p_folder_select_file_select' 'narrow folder selection history'
Need 'source\draw.c' 'vsnprintf(str, sizeof(str), format, va);' 'bounded debug formatting'
Reject 'source\draw.c' 'vsprintf(str, format, va);' 'unbounded debug formatting'

# 13.5 settings and memory changes.
Need 'source\ezkernelnew.c' '#define LAUNCHER_FILENAME_LEN 100' 'filename capacity constant'
Need 'source\ezkernelnew.c' '#define SAV_info_buffer SET_info_buffer' 'shared save/settings staging workspace'
Need 'source\ezkernelnew.c' 'static void Launcher_SettingsLoadCache(void)' 'one-pass settings cache'
Need 'source\ezkernelnew.c' 'Launcher_SettingsReadValue(LAUNCHER_SETTING_' 'ID-based settings reads'
Need 'source\ezkernelnew.c' 'Launcher_SettingsInvalidateCache();' 'settings cache invalidation'
Reject 'source\ezkernelnew.c' 'Launcher_SettingsReadValue("' 'runtime string-key settings reads'
Reject 'source\ezkernelnew.c' 'FM_FILE_FS pFilename_temp' 'obsolete insertion-sort record'
Reject 'source\ezkernelnew.c' 'TCHAR current_filename[200]' 'oversized current filename'

# Stable sorting and bounded movement.
Need 'source\ezkernelnew.c' 'Sort folders by stable-merging 16-bit indexes' 'stable folder merge sort'
Need 'source\ezkernelnew.c' 'stable-merging 16-bit indexes in EWRAM' '13.7b indexed file merge sort'
Need 'source\ezkernelnew.c' 'LauncherFileSortScratchFitsReadCache' '13.7b file-sort scratch range check'
Need 'source\ezkernelnew.c' 'LauncherFolderSortScratchFitsReadCache' 'folder sort scratch range check'
Need 'source\ezkernelnew.c' 'Launcher_CommitFileRecords(count);' '13.7b single-copy staged file lists'
Need 'source\ezkernelnew.c' '(launcher_favourite_count - index - 1) * LAUNCHER_FAVOURITE_PATH_LEN' 'single-block favourite removal'


# 13.6 restart / boot-menu input fixes.
Need 'source\ezkernelnew.c' 'u16 reset_key_mask =' '13.6 reset-chord Quick Start guard'
Need 'source\ezkernelnew.c' 'Launcher_FlushInputForModal();' '13.6 modal input drain'
Need 'source\ezkernelnew.c' 'else if(keysdown & KEY_B)' '13.6 fresh B press cancel'
Reject 'source\ezkernelnew.c' 'ignore_b_frames' 'obsolete fixed-frame B workaround'

# 13.7 v7.4 backports.
Need 'source\ez_define.h' '#define MAX_files     0x200' '512-file folder limit'
Need 'source\ezkernelnew.c' '#define LAUNCHER_FILE_LIST_PSRAM_ADDRESS 0x08FF0000' 'PSRAM directory list'
Need 'source\ezkernelnew.c' 'LauncherFileListFitsPSRAM' 'PSRAM directory size guard'
Need 'source\ezkernelnew.c' 'Launcher_StoreFileRecord' 'staged PSRAM directory writes'
Need 'source\ezkernelnew.c' 'SetPSRampage(0);' 'PSRAM page-zero restore'
Need 'source\ezkernelnew.c' 'Empty folders and virtual lists have no actionable entry.' 'empty list A-button guard'
Need 'source\ezkernelnew.c' '((absolute_index - folder_total) >= game_total_SD)' 'SD file selection bounds check'
Need 'source\Ezcard_OP.c' 'reset_flags |= RESET_EWRAM;' 'clean software-boot EWRAM reset'
Need 'source\draw.c' '#include "launcher_font_extended.h"' 'extended Latin font include'
Need 'source\draw.c' 'LAUNCHER_EXTENDED_LATIN[latin_index]' 'extended Latin glyph rendering'
Need 'source\launcher_font_extended.h' '#define LAUNCHER_EXTENDED_LATIN_GLYPH_COUNT 60' '60-glyph extended Latin table'


# 13.7a/13.7b cleanup / runtime optimizations.
Need 'source\ezkernelnew.c' '#define LAUNCHER_FILE_SORT_BYTES' '13.7b bounded shared-cache file-sort layout'
Need 'source\ezkernelnew.c' 'source[i] = (u16)i;' '13.7b lightweight index initialization'
Need 'source\ezkernelnew.c' 'output[i] = records[source[i]];' '13.7b final record materialization'
Need 'source\ezkernelnew.c' 'dmaCopy(output, pFilename_buffer, total * sizeof(FM_FILE_FS));' '13.7b single final PSRAM copy'
Reject 'source\ezkernelnew.c' 'dmaCopy(pFilename_buffer, buffer_a, bytes);' 'obsolete PSRAM-to-EWRAM pre-sort copy'
Reject 'source\ezkernelnew.c' 'blocknum * 0x20000 < 0x20000' 'broken final-block arithmetic'
Need 'source\ezkernelnew.c' 'memset(pReadCache + ret, 0xFF, 0x20000 - ret);' 'GBA ROM final-block padding'
Need 'source\NORflash_OP.c' 'GBApatch_NOR((u32*)pReadCache,filesize,blocknum)' 'NOR RTS full ROM-size identity'
Need 'source\NORflash_OP.c' 'memset(pReadCache + ret, 0xFF, 0x20000 - ret);' 'NOR final-block padding'
Need 'source\gba_nes_patch.h' 'static const unsigned char gba_nes_patch_bin[]' 'ROM-resident NES patch blob'
Need 'source\gba_rts_patch.s' 'LDR     R3, [R1],#4' 'optimized RTS SRAM write loop'
Need 'source\gba_rts_patch.s' 'ORR     R4, R4, R3, LSL #24' 'optimized RTS SRAM read loop'
Need 'source\gba_rts_only.s' 'LDR     R3, [R1],#4' 'optimized RTS-only SRAM write loop'
Need 'source\gba_sleep_patch.s' 'ldrh r2,[r2,#0x30]' '16-bit KEYINPUT read'
Reject 'source\gba_rts_patch.s' 'wSram_loop1:' 'old double-branch SRAM write loop'
Reject 'source\gba_rts_only.s' 'rSram_loop1:' 'old double-branch SRAM read loop'

# 13.7b behavior-preserving cleanup.
Need 'source\draw.h' 'void DrawHZText12(const char *str' 'const-correct text drawing API'
Need 'source\draw.c' 'static u32 DecodeUtf8Text12(const char *str' 'const-correct UTF-8 decoder'
Need 'source\ezkernelnew.c' 'Launcher_FormatSizeValue' 'lightweight file-size formatter'
Need 'source\ezkernelnew.c' '(launcher_last_launch_mode == LAST_LAUNCH_MODE_ADDON)' 'cached Quick Start launch mode'
Need 'source\gba_rts_patch.s' 'ORR     R4, R4, R3, LSL #8' 'compact combined RTS IO restore'
Need 'source\gba_rts_only.s' 'ORR     R4, R4, R3, LSL #8' 'compact RTS-only IO restore'
Reject 'source\ezkernelnew.c' 'u16 gl_color_cheat_count' 'unused cheat-count colour global'
Reject 'source\setwindow2.c' 'char msg[128];' 'unused settings text buffer'
Need 'source\setwindow.c' 'static const char *const launcher_key_names[10]' 'shared settings key-name table'
Need 'source\setwindow.c' 'Launcher_KeyName(edit_rtshotkey[0], K_L)' 'shared hotkey label resolver'
Reject 'source\ezkernelnew.c' 'launcher_list_art_idle_frames' 'obsolete zero-delay list-art idle state'
Reject 'source\ezkernelnew.c' 'LAUNCHER_LIST_ART_IDLE_LOAD_FRAMES' 'obsolete zero-delay list-art constant'
Need 'source\ezkernelnew.c' 'Launcher_CopyString(recents_return_path, sizeof(recents_return_path)' 'bounded Recent return-path copy'


# 13.7c behavior-preserving cleanup / UI hot paths.
Need 'source\ezkernelnew.c' 'Sort folders by stable-merging 16-bit indexes' '13.7c indexed folder merge sort'
Need 'source\ezkernelnew.c' 'Launcher_AsciiLower' '13.7c compact file-icon classifier'
Need 'source\ezkernelnew.c' 'LAUNCHER_FAVOURITE_FILE_CACHE_BYTES' '13.7c favourite file bitset'
Need 'source\ezkernelnew.c' 'Launcher_RebuildFavouriteFileCache' '13.7c favourite cache rebuild'
Need 'source\ezkernelnew.c' 'static u32 Launcher_IsFavouriteFileIndex(u32 file_index);' '13.7c favourite cache forward declaration'
Need 'source\ezkernelnew.c' 'static void Launcher_ReadClockHMS' '13.7c shared RTC clock reader'
Need 'source\ezkernelnew.c' 'temp[6] = (HH >= 12) ? ''P'' : ''A'';' '13.7c lightweight clock formatter'
Need 'source\ezkernelnew.c' 'u32 Check_file_type(const TCHAR *pfilename)' 'const-correct file-type lookup'
Need 'source\gba_rts_patch.s' 'strbne' '13.7c branchless direct W8 conditional store'
Reject 'source\ezkernelnew.c' '__attribute__((unused))' 'explicitly dead launcher helpers'
Reject 'source\ezkernelnew.c' 'launcher_vertical_folder_label_dirty' 'disabled vertical folder-label state'
Reject 'source\Ezcard_OP.c' 'image_bin_size2' 'unused firmware size constant'
Reject 'source\lang.c' 'en_please_wait' 'unused English wait string'
Reject 'source\lang.c' 'en_no_roms' 'unused English no-ROM string'
Reject 'source\lang.c' 'en_LSELECT_help' 'unused English legacy help string'
Reject 'source\lang.c' 'th_lang[]' 'unused Thai language-name duplicate'

Reject 'source\gba_rts_patch.s' '[r7,#208]' 'decimal IME typo'
Reject 'source\gba_rts_patch.s' '0x0E008500' 'wrong DMA context pointer'
# 13.7d1: .arm does not select unified syntax. Require the declaration
# before the first include/instruction, not merely somewhere near reset_code.
$RuntimeText = (Read-All 'source\gba_rts_patch.s').Replace("`r`n", "`n")
if (-not $RuntimeText.StartsWith(".syntax unified`n", [System.StringComparison]::Ordinal)) {
    throw '13.7d1: source\gba_rts_patch.s must start with .syntax unified'
}
Need 'Build-DS-Style-Enhanced.cmd' 'Build-RTS-Assembly-Preflight.ps1' '13.7d1 native GNU assembler preflight'

# 13.7f external-only game compatibility database.
Need 'source\GBApatch.c' 'AutoPatch_PrepareProfile' '13.7f external profile preparation'
Need 'source\GBApatch.c' 'AutoPatch_VerifyIrqVariant' 'external IRQ variant guard'
Need 'source\GBApatch.c' 'AutoPatch_AppendDeferredRecords' 'FORMAT=2 ADD32 pipeline'
Need 'source\GBApatch.c' 'AutoPatch_ApplyTrimOverride' 'FORMAT=2 trim override'
Need 'source\GBApatch.c' 'AutoPatch_ApplyFixedWrites' 'FORMAT=2 direct ROM writes'
Need 'source\GBApatch.c' 'AutoPatch_ApplySearchWrites' 'FORMAT=2 bounded search writes'
Need 'source\ezkernelnew.c' 'AutoPatch_PrepareProfile(pfilename, GAMECODE, gamefilesize)' 'profile prepared for clean/addon/NOR paths'
Need 'source\ezkernelnew.c' 'use_external_patch_engine(pfilename,GAMECODE,gamefilesize)' 'external fixed-address IRQ selection'
Need 'tools\export_external_gba_patches.py' 'FORMAT=2' 'v2 patch-folder exporter'
Need 'tools\export_external_gba_patches.py' 'PATCH16=' 'direct-write exporter'
Need 'tools\export_external_gba_patches.py' 'SEARCH32PAIR=' 'search-patch exporter'
Need 'tools\validate_external_gba_patches.py' 'active_special_unique_codes' 'v2 patch-folder validator'
Reject 'source\GBApatch.c' 'Patch_SpecialROM_sleepmode' 'hard-coded sleep compatibility switch'
Reject 'source\GBApatch.c' 'Patch_SpecialROM_TrimSize' 'hard-coded trim compatibility switch'
Reject 'source\GBApatch.c' 'PatchDragonBallZ' 'hard-coded Dragon Ball/Top Gun compatibility switch'
Reject 'source\GBApatch.c' 'Patch_somegame' 'hard-coded 2GBP compatibility search'
Reject 'source\GBApatch.c' 'Check_Fire_Emblem' 'dead commented Fire Emblem block'
# 13.7e1 game-RAM preservation and patch-selection guards.
Reject 'source\GBApatch.c' 'spend_address = Get_spend_address(address)' 'no game-stack borrowing'
Need 'source\GBApatch.c' 'extern u16 gl_engine_sel;' 'correct shared engine selector declaration'
Need 'source\GBApatch.c' 'memcmp(GAMECODE,"BPRE",4)' 'FireRed cache bypass'
Need 'source\gba_rts_patch.s' 'exclusive end of private IRQ IF stack' 'IF stack bound follows writable stack'
Reject 'source\gba_rts_patch.s' 'RTS_ADRL		r12,cheat_condition_stack' 'no conditional stack in ROM'
Reject 'source\gba_rts_patch.s' 'delay_loop:' 'obsolete post-commit-only delay'

# External patch database is data-only; active special rules and disabled Fire Emblem research are packaged under SD-Card-Addons for SD installation.
Need 'SD-Card-Addons\SYSTEM\PATCHES\MANIFEST.json' '"format": 2' 'FORMAT=2 patch manifest'
Need 'SD-Card-Addons\SYSTEM\PATCHES\FORMAT.txt' 'FORMAT=2' 'FORMAT=2 patch documentation'
Need 'SD-Card-Addons\SYSTEM\PATCHES\GBA\B\P\BPRE.patch' 'FORMAT=2' 'FireRed v2 patch profile'
Need 'SD-Card-Addons\SYSTEM\PATCHES\GBA\B\P\BPRE.patch' 'IRQ32=007039BC' 'FireRed Rev1 variant retained'
Need 'SD-Card-Addons\SYSTEM\PATCHES\GBA\A\L\ALFE.patch' 'PATCH16=003B8E9E,1001' 'Dragon Ball direct patch externalized'
Need 'SD-Card-Addons\SYSTEM\PATCHES\GBA\2\G\2GBP.patch' 'SEARCH32PAIR=' '2GBP search externalized'
Need 'SD-Card-Addons\SYSTEM\PATCHES\GBA\A\E\AE7E.patch' '# STATUS=DISABLED_LEGACY' 'Fire Emblem research preserved disabled'
Need 'tools\compatibility_tables\gba_special_legacy.json' '"disabled_fire_emblem"' 'archival special compatibility source'

# RTS3: independent hardware-checkpoint core with explicit fidelity limits.
Need 'source\gba_rts_only.s' 'bl rts3_validate' 'RTS-only RTS3 preflight'
Need 'source\gba_rts_patch.s' 'bl rts3_validate' 'combined RTS3 preflight'
Need 'source\gba_rts3_defs.inc' '0x138A0001' 'RTS3 ARM ABI'
Need 'source\rts3_identity.h' '0x138A0001' 'RTS3 C ABI'
Need 'source\rts3_identity.h' 'RTS3_HEADER_SIZE 256u' '256-byte immutable manifest'
Need 'source\GBApatch.c' 'Rts3FinalizeRuntime16(' 'VRAM-safe RTS3 identity finalizer'
Need 'source\GBApatch.c' 'cache[trailer + 12] == RTS3_KERNEL_ABI' 'RTS3 runtime cache invalidation'
Need 'source\gba_rts3_defs.inc' 'RTS3_WORK_BASE = 0x06017C00' 'isolated backed-up VRAM stack'
Need 'source\gba_rts3_defs.inc' 'RTS3_REGION_COUNT = 18' 'low-window payload chunks'
Need 'source\gba_rts3_core.inc' 'rts3_validate_cpu:' 'BIOS-frame and bank validation'
Need 'source\gba_rts3_core.inc' 'rts3_load_check:' 'destination CRC validation'
Need 'source\gba_rts3_core.inc' 'rts3_tail_failed:' 'final VRAM-tail integrity gate'
Need 'source\gba_rts3_core.inc' 'rts3_irq_return_probe:' 'first game IRQ return witness'
Need 'source\gba_rts3_core.inc' 'strh r10,[r0,#8]' 'final IME handoff write'
Need 'source\gba_rts3_core.inc' 'rts3_restore_menu_io:' 'separate normal exit IO policy'
Need 'source\gba_rts3_menu.inc' 'sprite-only menu' 'non-affine menu renderer'
Need 'source\ezkernelnew.c' 'page += 0x08' '32 KiB RTS import windows'
Need 'tools\inspect_rts3.py' 'ABI = 0x138A0001' 'RTS3 file inspector'
Reject 'source\gba_rts_patch.s' 'bl rts2_' 'no live RTS2 call path'
Reject 'source\gba_rts_only.s' 'bl rts2_' 'no live RTS2 call path'
Write-Host 'PASS: DS Style 7.4c structural source validation'
