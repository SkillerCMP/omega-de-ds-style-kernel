#!/usr/bin/env python3
from pathlib import Path
import re, struct, json, argparse

def parse_legacy(path):
    rows=[]
    for ln,line in enumerate(path.read_text('utf-8').splitlines(),1):
        left=line.split('//',1)[0]
        vals=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',left)]
        if not vals: continue
        if vals[0]==0xFFFFFFFF: break
        if len(vals)<2: raise AssertionError((ln,line))
        count=vals[1]; offs=vals[2:2+count]
        if len(offs)!=count: raise AssertionError((ln,count,len(offs)))
        raw=vals[0].to_bytes(4,'little')
        key=raw.decode('ascii') if all((48<=c<=57) or (65<=c<=90) or (97<=c<=122) for c in raw) else 'HEX'+raw.hex().upper()
        rows.append((key,[x*4 for x in offs]))
    return rows

def patch_path(root,key):
    if key.startswith('HEX'): return root/'_HEX'/f'{key[3:]}.patch'
    return root/key[0]/key[1]/f'{key}.patch'

def parse_fields(value,count):
    parts=value.split(','); assert len(parts)==count,(value,count)
    return tuple(int(x,16) for x in parts)

def parse_patch(path):
    game=None; variants=[]; cur=None; fmt=None; fire_comments=[]
    for raw in path.read_text('ascii').splitlines():
        line=raw.strip()
        if not line: continue
        if line.startswith('#'):
            if 'FIRE_EMBLEM_' in line or 'STATUS=DISABLED_LEGACY' in line or 'PATCH_BLOB' in line: fire_comments.append(line)
            continue
        if line.startswith('FORMAT='): fmt=int(line[7:],16)
        elif line.startswith('GAME='): game=line[5:]
        elif line.startswith('GAMEHEX='): game='HEX'+line[8:]
        elif line.startswith('VARIANT='):
            assert cur is None
            cur={'number':int(line[8:],16),'irq':[],'no_irq':False,'add32':[],'patch16':[],'patch32':[],'trim':None,'search':[]}
        elif line=='NO_IRQ=1': assert cur is not None and not cur['irq'];cur['no_irq']=True
        elif line.startswith('IRQ32='): assert cur is not None and not cur['no_irq'];cur['irq'].append(int(line[6:],16))
        elif line.startswith('ADD32='): assert cur is not None;cur['add32'].append(parse_fields(line[6:],2))
        elif line.startswith('PATCH16='): assert cur is not None;cur['patch16'].append(parse_fields(line[8:],2))
        elif line.startswith('PATCH32='): assert cur is not None;cur['patch32'].append(parse_fields(line[8:],2))
        elif line.startswith('TRIM='): assert cur is not None and cur['trim'] is None;cur['trim']=int(line[5:],16)
        elif line.startswith('SEARCH32PAIR='): assert cur is not None;cur['search'].append(parse_fields(line[13:],6))
        elif line=='END': assert cur is not None;variants.append(cur);cur=None
        else: raise AssertionError((path,line))
    assert fmt==2 and game and cur is None
    return game,variants,fire_comments

def select_for_rom(root,rompath):
    data=rompath.read_bytes(); raw=data[0xAC:0xB0]
    key=raw.decode('ascii') if all((48<=c<=57) or (65<=c<=90) or (97<=c<=122) for c in raw) else 'HEX'+raw.hex().upper()
    p=patch_path(root,key)
    if not p.exists(): return key,[]
    _,vars,_=parse_patch(p);matches=[]
    for v in vars:
        applied=v['irq'][:32]
        ok=v['no_irq'] or (bool(applied) and all(off%4==0 and off+4<=len(data) and struct.unpack_from('<I',data,off)[0] in (0x03007FFC,0x03FFFFFC) for off in applied))
        if ok:matches.append({'variant':v['number'],'offsets':applied,'add32':v['add32'],'patch16':v['patch16'],'trim':v['trim'],'search':v['search']})
    return key,matches

def main():
    ap=argparse.ArgumentParser();ap.add_argument('source_root');ap.add_argument('--rom',action='append',default=[]);a=ap.parse_args()
    root=Path(a.source_root);legacy=root/'tools/compatibility_tables/reset_table_legacy.h';patchroot=root/'SD-Card-Addons/SYSTEM/PATCHES/GBA';special=json.loads((root/'tools/compatibility_tables/gba_special_legacy.json').read_text())
    rows=parse_legacy(legacy);by={}
    for k,o in rows:by.setdefault(k,[]).append(o)
    active=set(special['deferred_add32'])|set(special['trim'])|set(special['direct16'])|set(special['search32pair'])
    expected_codes=set(by)|active|set(special['disabled_fire_emblem'])
    files=list(patchroot.rglob('*.patch'));assert len(files)==len(expected_codes),(len(files),len(expected_codes))
    for key in expected_codes:
        p=patch_path(patchroot,key);assert p.exists(),p
        game,variants,fire=parse_patch(p);assert game==key,(game,key)
        reset_variants=by.get(key,[])
        assert len(variants)==max(1,len(reset_variants)),(key,len(variants),len(reset_variants))
        if not reset_variants:reset_variants=[[]]
        for v,irq in zip(variants,reset_variants):
            assert v['irq']==irq,(key,v['irq'],irq)
            assert v['no_irq']==(len(irq)==0),(key,v['no_irq'],irq)
            exp_add=[(x['offset'],x['value']) for x in special['deferred_add32'].get(key,{}).get('add32',[])]
            assert v['add32']==exp_add,(key,'add32',v['add32'],exp_add)
            exp16=[(x['offset'],x['value']) for x in special['direct16'].get(key,[])]
            assert v['patch16']==exp16,(key,'patch16')
            assert v['patch32']==[]
            exptrim=special['trim'].get(key,{}).get('trim')
            assert v['trim']==exptrim,(key,'trim',v['trim'],exptrim)
            expsearch=[tuple(x[n] for n in ('start','length','word0','word1','write_delta','replacement')) for x in special['search32pair'].get(key,[])]
            assert v['search']==expsearch,(key,'search')
        if key in special['disabled_fire_emblem']:
            assert any('STATUS=DISABLED_LEGACY' in x for x in fire),key
            assert any('FIRE_EMBLEM_HOOK32=' in x for x in fire),key
    assert len(rows)==2814
    max_size=max(p.stat().st_size for p in files);assert max_size<0x20000
    g=(root/'source/GBApatch.c').read_text('latin1');e=(root/'source/ezkernelnew.c').read_text('latin1')
    for forbidden in ['reset_table_packed','use_internal_engine','Patch_SpecialROM_sleepmode','Patch_SpecialROM_TrimSize','PatchDragonBallZ','Patch_somegame','Check_Fire_Emblem']:
        assert forbidden not in g,forbidden
    assert '#include "reset_table.h"' not in g
    for required in ['AutoPatch_PrepareProfile','AutoPatch_AppendDeferredRecords','AutoPatch_ApplyTrimOverride','AutoPatch_ApplyFixedWrites','AutoPatch_ApplySearchWrites']:
        assert required in g,required
    assert 'AutoPatch_PrepareProfile(pfilename, GAMECODE, gamefilesize)' in e
    results={'format':2,'legacy_rows':len(rows),'patch_files':len(files),'duplicate_rows':len(rows)-len(by),'max_patch_bytes':max_size,
             'active_special_unique_codes':len(active),'deferred_add32_codes':len(special['deferred_add32']),'deferred_add32_records':sum(len(v['add32']) for v in special['deferred_add32'].values()),
             'trim_codes':len(special['trim']),'direct16_codes':len(special['direct16']),'direct16_records':sum(len(v) for v in special['direct16'].values()),'search_codes':len(special['search32pair']),
             'disabled_fire_emblem_codes':len(special['disabled_fire_emblem']),'roms':[]}
    for r in a.rom:
        key,matches=select_for_rom(patchroot,Path(r));results['roms'].append({'rom':Path(r).name,'game':key,'matches':[{**m,'offsets':[f'{x:08X}' for x in m['offsets']]} for m in matches]})
    print(json.dumps(results,indent=2))
if __name__=='__main__':main()
