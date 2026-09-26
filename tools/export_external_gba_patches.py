#!/usr/bin/env python3
from pathlib import Path
import re, collections, json, shutil, hashlib

src_root=Path(__file__).resolve().parents[1]
legacy=src_root/'tools/compatibility_tables/reset_table_legacy.h'
special_path=src_root/'tools/compatibility_tables/gba_special_legacy.json'
out=src_root/'SD-Card-Addons/SYSTEM/PATCHES/GBA'
if out.exists(): shutil.rmtree(out)
out.mkdir(parents=True)
text=legacy.read_text('utf-8')
special=json.loads(special_path.read_text('utf-8'))
rows=[]
for line_no,line in enumerate(text.splitlines(),1):
    m=re.search(r'^(.*?),\s*//\s*(.*)$', line.strip())
    if not m: continue
    vals=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',m.group(1))]
    if not vals: continue
    if vals[0]==0xFFFFFFFF: break
    if len(vals)<2: raise SystemExit(('bad',line_no,line))
    code_int,count=vals[0],vals[1]
    offs=vals[2:2+count]
    if len(offs)!=count: raise SystemExit(('count',line_no,count,len(offs)))
    raw=code_int.to_bytes(4,'little')
    printable=all((48 <= c <= 57) or (65 <= c <= 90) or (97 <= c <= 122) for c in raw)
    code=raw.decode('ascii') if printable else f'HEX{raw.hex().upper()}'
    display=raw.decode('ascii') if printable else None
    name=m.group(2).strip()
    rows.append({'line':line_no,'code':code,'display':display,'code_int':code_int,'offset_words':offs,'name':name})
by=collections.OrderedDict()
for r in rows: by.setdefault(r['code'],[]).append(r)

active_codes=set(special['deferred_add32'])|set(special['trim'])|set(special['direct16'])|set(special['search32pair'])
fire_codes=set(special['disabled_fire_emblem'])
for code in sorted(active_codes|fire_codes):
    by.setdefault(code,[])

def write_ops(lines, code):
    add=special['deferred_add32'].get(code)
    if add:
        for c in add.get('source_comments',[]):
            if c: lines.append(f'# LEGACY_ADD32_SOURCE={c}')
        for op in add['add32']:
            lines.append(f'ADD32={op["offset"]:08X},{op["value"]:08X}')
    tr=special['trim'].get(code)
    if tr:
        for c in tr.get('source_comments',[]):
            if c: lines.append(f'# LEGACY_TRIM_SOURCE={c}')
        lines.append(f'TRIM={tr["trim"]:08X}')
    for op in special['direct16'].get(code,[]):
        lines.append(f'PATCH16={op["offset"]:08X},{op["value"]:04X}')
    for op in special['search32pair'].get(code,[]):
        lines.append('SEARCH32PAIR=' + ','.join([
            f'{op["start"]:08X}',f'{op["length"]:08X}',f'{op["word0"]:08X}',f'{op["word1"]:08X}',
            f'{op["write_delta"]:08X}',f'{op["replacement"]:08X}']))

def append_fire_comments(lines, code):
    fe=special['disabled_fire_emblem'].get(code)
    if not fe: return
    lines += [
        '# ============================================================',
        '# STATUS=DISABLED_LEGACY',
        '# DO_NOT_ENABLE_WITHOUT_HARDWARE_TESTING',
        '# Legacy Check_Fire_Emblem() research preserved as comments only.',
        f'# TITLE={fe["title"]}',
        f'# PATCH_BLOB={fe["blob"]}',
        f'# PATCH_ADDRESS={fe["patchaddress"]:08X}',
        '# HOOK_REPLACEMENT_WORD0=47004800',
    ]
    base=0x08000000+fe['patchaddress']
    for hook,delta in zip(fe['hooks'],fe['code2_deltas']):
        lines.append(f'# FIRE_EMBLEM_HOOK32={hook:08X},47004800,{base+delta:08X}')
    if 'modify' in fe:
        lines.append(f'# PATCH_BLOB_MODIFY_VALUE={fe["modify"]:08X}')
    lines += ['# AUTO_SAVE_LEGACY_BEHAVIOR=DISABLE_WHEN_PATCH_ACTIVE','# ============================================================','']

for code, variants in by.items():
    if code.startswith('HEX'):
        d=out/'_HEX'; filename=f'{code[3:]}.patch'
    else:
        d=out/code[0]/code[1]; filename=f'{code}.patch'
    d.mkdir(parents=True,exist_ok=True)
    p=d/filename
    lines=[
        '# DS Style GBA Auto Patch',
        '# Plain-text external game compatibility profile.',
        '# IRQ32 uses ROM BYTE offsets and is validated against 03007FFC/03FFFFFC.',
        '# ADD32 feeds the existing deferred Add2()/iPatchInfo2 pipeline.',
        '# PATCH16/PATCH32 are direct ROM writes; TRIM overrides patch placement.',
        '# SEARCH32PAIR is a bounded legacy search/write operation.',
        'FORMAT=2', (f'GAME={code}' if not code.startswith('HEX') else f'GAMEHEX={code[3:]}'), '',
    ]
    if not variants:
        variants=[{'name':'external special compatibility entry','offset_words':[]}]
    for i,r in enumerate(variants,1):
        lines += [f'VARIANT={i}', f'# SOURCE={r["name"]}']
        if not r['offset_words']:
            lines.append('NO_IRQ=1')
        else:
            for w in r['offset_words']:
                lines.append(f'IRQ32={w*4:08X}')
        write_ops(lines, code)
        lines += ['END','']
    append_fire_comments(lines, code)
    p.write_text('\n'.join(lines), encoding='ascii', newline='\n')

manifest={
 'format':2,'reset_source':'tools/compatibility_tables/reset_table_legacy.h',
 'special_source':'tools/compatibility_tables/gba_special_legacy.json',
 'legacy_rows':len(rows),'game_files':len(by),
 'duplicate_rows':len(rows)-len({r['code'] for r in rows}),
 'game_codes_with_multiple_variants':sum(1 for v in by.values() if len(v)>1),
 'max_variants':max(map(len,[v for v in by.values() if v] or [[1]])),
 'max_offsets':max(len(r['offset_words']) for r in rows),
 'deferred_add32_codes':len(special['deferred_add32']),
 'deferred_add32_records':sum(len(v['add32']) for v in special['deferred_add32'].values()),
 'trim_codes':len(special['trim']),
 'direct16_codes':len(special['direct16']),
 'direct16_records':sum(len(v) for v in special['direct16'].values()),
 'search_codes':len(special['search32pair']),
 'disabled_fire_emblem_codes':len(special['disabled_fire_emblem']),
 'active_special_unique_codes':len(active_codes),
 'files': {code: max(1,len(v)) for code,v in by.items()}
}
(out.parent/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
format_doc=out.parent/'FORMAT.txt'
format_doc.write_text('''DS Style - External GBA Auto Patch text format v2

Required header:
  FORMAT=2
  GAME=BPRE

For non-alphanumeric four-byte game codes the exporter uses GAMEHEX instead.
Variants remain in legacy priority order and end with END.

Directives:
  IRQ32=offset
      ROM BYTE offset of an IRQ pointer. A candidate variant is accepted only
      when every applicable IRQ32 location contains 03007FFC or 03FFFFFC.
      Accepted entries become Add2(offset / 4, 03007FF4).
  NO_IRQ=1
      This variant intentionally has no reset/IRQ relocation sites.
  ADD32=offset,value
      Deferred 32-bit compatibility record sent through Add2()/iPatchInfo2.
  PATCH16=offset,value
  PATCH32=offset,value
      Direct boot-time ROM writes.
  TRIM=size
      Exact legacy trim-size override.
  SEARCH32PAIR=start,length,word0,word1,write_delta,replacement
      Bounded pair search/write operation.

Lines beginning with # and blank lines are ignored. Disabled Fire Emblem
research is preserved only as # comments and is never executed.

Selection:
  - The first IRQ-safe variant wins.
  - A missing file or a file with no safe IRQ variant routes normal IRQ
    relocation to the generic ROM scanner.
  - Game-specific ADD32/PATCH/TRIM/SEARCH compatibility behavior is external;
    install the v2 PATCHES database for full compatibility behavior.

The compiled reset table and the old game-specific switch statements are not
runtime compatibility sources.
''',encoding='ascii',newline='\n')
readme=out.parent/'README.txt'
readme.write_text('''DS Style - External GBA Auto Patches v2\n\nCopy the PATCHES folder to /SYSTEM on the SD card.\n\nLayout:\n  /SYSTEM/PATCHES/GBA/<first game-code char>/<second game-code char>/<GAME>.patch\n\nFORMAT=2 keeps the original reset/IRQ variants and also externalizes the active\ngame-specific compatibility data that used to be hard-coded in GBApatch.c.\n\nCommands:\n  IRQ32=offset\n      Deferred IRQ relocation. The loader validates the original word is\n      03007FFC or 03FFFFFC, then adds replacement 03007FF4.\n  NO_IRQ=1\n      Variant intentionally has no reset/IRQ relocation offsets.\n  ADD32=offset,value\n      Deferred 32-bit record passed through Add2()/iPatchInfo2, preserving the\n      old Patch_SpecialROM_sleepmode behavior.\n  PATCH16=offset,value\n  PATCH32=offset,value\n      Direct boot-time ROM writes. PATCH16 holds the former Dragon Ball/Top Gun\n      compatibility edits.\n  TRIM=size\n      Exact legacy iTrimSize override.\n  SEARCH32PAIR=start,length,word0,word1,write_delta,replacement\n      Bounded legacy pair search. Currently used for 2GBP only.\n\nThe legacy Fire Emblem Check_Fire_Emblem() research was already disabled in the\nkernel. It is preserved only as # commented lines in the relevant .patch files\nand is not parsed or executed.\n\nThe compiled reset table and game-specific switch statements are no longer the\nruntime source of compatibility data. If a v2 file is absent or has no safe IRQ\nvariant, the generic ROM IRQ scanner remains the safety fallback.\n\nCompatibility note: iPatchInfo2 still has EMax=32. Two legacy reset rows contain\nmore than 32 IRQ offsets; v2 intentionally validates/applies the same first 32\nrecords, preserving inherited behavior.\n''',encoding='ascii',newline='\n')

checksums=[]
for patch_file in sorted(out.rglob('*.patch')):
    digest=hashlib.sha256(patch_file.read_bytes()).hexdigest().upper()
    checksums.append(f"{digest}  {patch_file.relative_to(out.parent).as_posix()}")
(out.parent/'SHA256SUMS.txt').write_text('\n'.join(checksums)+'\n', encoding='ascii', newline='\n')

print(json.dumps({k:v for k,v in manifest.items() if k!='files'},indent=2))
for code in ['BPRE','BPGE','B8ME','ALFE','2GBP','AE7E']:
    p=out/code[0]/code[1]/f'{code}.patch'
    print('\n---',code,'---')
    print(p.read_text() if p.exists() else 'MISSING')
