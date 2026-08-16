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

# Cumulative v7.3 through 13.5 structural source checks.
# This clean validator is intentionally self-contained: no Python, tools, or test harness files are required.
Need 'source\launcher_version.h' '#define LAUNCHER_VERSION_TEXT "v7.3"' 'DS Style v7.3 version marker'
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
Need 'source\showcht.c' 'char buf[MAX_BUF_LEN + 1]EWRAM_BSS;' 'shared parser buffer'
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

Need 'Makefile' '-Os' 'size-optimized compiler flag'
Need 'Makefile' '-ffunction-sections' 'function section flag'
Need 'Makefile' '-fdata-sections' 'data section flag'
Need 'Makefile' '--gc-sections' 'linker garbage collection'
Need 'source\ff15\ffconf.h' 'FF_PRINT_LLI' 'FatFs formatter configuration'
Need 'source\ff15\ffconf.h' 'FF_PRINT_FLOAT' 'FatFs formatter configuration'
Need 'source\ezkernelnew.c' '#define LAUNCHER_LIST_ART_CACHE_COUNT 2' 'two-slot List + Art cache'
Need 'source\ezkernelnew.c' '#define launcher_start_preview_cache launcher_thumbnail_workspace.start_preview' 'shared Start preview workspace'
Need 'source\ezkernelnew.c' 'launcher_custom_thumb_manifest_hash[LAUNCHER_CUSTOM_THUMB_MANIFEST_MAX]' 'single active manifest table'
Need 'source\ezkernelnew.c' 'Launcher_DrawThemeTopbarClip' 'compact top-bar renderer'
Need 'source\launcher_theme_assets.h' 'launcher_topbar_patterns.h' 'compact top-bar theme table'
Need 'source\launcher_topbar_patterns.h' 'LAUNCHER_TOPBAR_PATTERN_COUNT' 'compact top-bar patterns'
Need 'Grit\Build Skin Files.ps1' 'Write-CompactTopbarHeader' 'compact top-bar generator'

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

Need 'source\reset_table.h' '#define RESET_TABLE_GAME_COUNT 2768u' 'packed reset game count'
Need 'source\reset_table.h' '#define RESET_TABLE_PACKED_SIZE 33506u' 'packed reset size'
Need 'source\reset_table.h' 'reset_table_packed[]' 'packed reset data'
Need 'source\saveMODE.h' '#define SAVE_MODE_PACKED_ENTRY_COUNT 2768u' 'packed save-mode count'
Need 'source\saveMODE.h' 'save_mode_packed_table[]' 'packed save-mode data'
Need 'source\GBApatch.c' 'ResetTable_ReadLE24' '24-bit reset decoder'
Need 'source\GBApatch.c' 'const u8 *cursor = reset_table_packed;' 'direct reset ROM lookup'
Reject 'source\GBApatch.c' 'dmaCopy((void*)reset_table' 'old reset cache copy'
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

# 13.5 stable merge sort and bounded movement.
Need 'source\ezkernelnew.c' 'Sort folders with a stable bottom-up merge sort' 'stable folder merge sort'
Need 'source\ezkernelnew.c' 'Sort files with the same stable merge algorithm' 'stable file merge sort'
Need 'source\ezkernelnew.c' 'LauncherSortScratchFitsReadCache' 'file sort scratch range check'
Need 'source\ezkernelnew.c' 'LauncherFolderSortScratchFitsReadCache' 'folder sort scratch range check'
Need 'source\ezkernelnew.c' '(launcher_favourite_count - index - 1) * LAUNCHER_FAVOURITE_PATH_LEN' 'single-block favourite removal'


Write-Host 'PASS: DS Style v7.3 Enhanced 13.5 clean standalone source validation'
