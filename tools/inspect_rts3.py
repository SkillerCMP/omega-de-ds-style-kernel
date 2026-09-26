#!/usr/bin/env python3
"""Read-only RTS3 inspector. No ROM, emulator or third-party package needed.
File integrity is NOT proof that the physical GBA resumed correctly. CPU checks
here are reporting aids; the target loader additionally checks installed identity.
Usage: python tools/inspect_rts3.py "Game.rts" [--json] [--output report.json]
"""
from __future__ import annotations
import argparse, hashlib, json, struct, sys, zlib
from pathlib import Path
ABI = 0x138A0001
FILE_BYTES = 0x70000
HEADER = 0x68800
DIAG = 0x68C00
REGIONS = [(f'EWRAM_{i}',i*0x8000,0x8000) for i in range(8)] + [
 ('IWRAM',0x40000,0x8000),('Palette',0x48000,0x400),
 ('VRAM_0',0x50000,0x8000),('VRAM_1',0x58000,0x8000),('VRAM_2',0x60000,0x7C00),
 ('VRAM_workspace_tail',0x67C00,0x400),('OAM',0x68000,0x400),('CPU',0x68400,0x100),
 ('Readable_HW',0x68500,0x80),('Readable_IO',0x68600,0x80)]
STAGES={0x100:'SAVE_STARTED',0x110:'SAVE_CHUNK_WRITTEN',0x180:'SAVE_COMMITTED_TO_SRAM',
 0x200:'MANIFEST_OK',0x210:'SOURCE_CHUNK_VERIFIED',0x280:'PREFLIGHT_OK',
 0x300:'RESTORING_CHUNK',0x310:'DESTINATION_CHUNK_VERIFIED',0x380:'RAM_AND_STAGING_VERIFIED',
 0x400:'ABOUT_TO_CHAIN_GAME_IRQ',0x500:'FIRST_GAME_IRQ_RETURNED',
 0xE00:'PREFLIGHT_REJECTED',0xE10:'DESTINATION_CRC_FAILED',0xE11:'VRAM_TAIL_CRC_FAILED'}
REJECTS={1:'footer/format',2:'installed identity/fixed header',3:'header CRC',4:'CRC complement/reserved bytes',5:'CPU/BIOS frame/HW consistency'}

def u32(b:bytes,off:int)->int:return struct.unpack_from('<I',b,off)[0]
def u16(b:bytes,off:int)->int:return struct.unpack_from('<H',b,off)[0]
def hx(v:int)->str:return f'{v:08X}'

def inspect(data:bytes)->dict:
 out={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper(),'format':'unknown','structurally_valid':False,'errors':[]}
 if len(data)!=FILE_BYTES:
  out['errors'].append(f'Expected exactly 0x70000 bytes, received {len(data):#x}.');return out
 if data[HEADER:HEADER+8]!=b'EZRTS3K0':
  out['format']='not RTS3 (older or incompatible state)';out['errors'].append('RTS3 header not present. No conversion or ABI patching is safe.');return out
 out['format']='RTS3'
 h=bytearray(data[HEADER:HEADER+0x100]);stored=u32(h,0x3C);struct.pack_into('<I',h,0x3C,0)
 out['abi']=hx(u32(h,0x34));out['header_crc']={'stored':hx(stored),'actual':hx(zlib.crc32(h)),'matches':stored==zlib.crc32(h)}
 for off,value in [(8,3),(12,0x100),(16,FILE_BYTES),(0x28,1),(0x2C,18),(0x30,0x000F0001),(0x34,ABI)]:
  if u32(h,off)!=value:out['errors'].append(f'Unexpected manifest field at {off:#x}.')
 if not out['header_crc']['matches']:out['errors'].append('Header CRC mismatch.')
 if any(h[0xD0:]):out['errors'].append('Reserved header bytes are not zero.')
 footer=data[0x6FFF0:]
 if footer[:8] not in [b'EZRTSO03',b'EZRTSC03']:out['errors'].append('Invalid or incomplete commit footer.')
 if footer[8:]!=h[0x14:0x1C]:out['errors'].append('Footer and header ROM identities disagree.')
 out['game_code']=bytes(h[0x14:0x18]).decode('ascii',errors='replace')
 out['rom_bytes']=u32(h,0x18);out['rom_header_crc']=hx(u32(h,0x1C))
 out['runtime']={'crc':hx(u32(h,0x20)),'address':hx(u32(h,0x24)),'bytes':u32(h,0x38),'identity_verified_against_ROM':False}
 out['regions']=[]
 for i,(name,off,size) in enumerate(REGIONS):
  crc,comp=struct.unpack_from('<II',h,0x40+i*8);actual=zlib.crc32(data[off:off+size]);good=actual==crc and comp==(crc^0xFFFFFFFF)
  out['regions'].append({'index':i,'name':name,'offset':f'{off:05X}','bytes':size,'expected_crc':hx(crc),'actual_crc':hx(actual),'complement_matches':comp==(crc^0xFFFFFFFF),'matches':good})
  if not good:out['errors'].append(f'Payload chunk {i} ({name}) failed its integrity check.')
 c=data[0x68400:0x68500];hw=data[0x68500:0x68580]
 out['cpu']={'gprs':[hx(u32(c,i*4)) for i in range(16)],'cpsr':hx(u32(c,0x40)),'callback_cpsr':hx(u32(c,0x44)),'irq_callback_sp':hx(u32(c,0x48)),'bios_return':hx(u32(c,0x4C)),'original_irq_handler':hx(u32(c,0xB4)),'installed_irq_hook':hx(u32(c,0xB8)),'bios_frame':[hx(u32(c,0x50+i*4)) for i in range(6)]}
 if c[0xC0:0xC4]!=b'CPU3' or u32(c,0xBC)!=0x3F:out['errors'].append('CPU schema mismatch.')
 if hw[:4]!=b'RHW3' or u32(hw,0x3C)!=3:out['errors'].append('HW schema mismatch.')
 out['hardware']={'DMA_controls':[f'{u16(hw,8+2*i):04X}' for i in range(4)],'timer_count_observations':[u16(hw,0x10+2*i) for i in range(4)],'timer_controls':[f'{u16(hw,0x18+2*i):04X}' for i in range(4)],'IME':u16(hw,0x20),'IE':f'{u16(hw,0x22):04X}','IF_observed':f'{u16(hw,0x24):04X}','VCOUNT':u16(hw,0x2C)}
 d=data[DIAG:DIAG+0x100];stage=u32(d,8);detail=u32(d,12)
 diag_ok=d[:4]==b'RTD3' and u32(d,4)==ABI and u32(d,0xFC)==zlib.crc32(d[:0xFC])
 out['diagnostic']={'self_crc_valid':diag_ok,'stage':f'{stage:04X}','stage_name':STAGES.get(stage,'UNKNOWN'),'detail':detail,'meaning':REJECTS.get(detail,f'payload chunk {detail-0x100}' if 0x100<=detail<0x112 else '') if stage==0xE00 else '', 'is_proof_of_gameplay':False,'SD_flush_confirmed':False}
 out['limitations']=['DMA programming/progress is not captured; LOAD restarts retained live latches.','Timer reload/phase is not captured; counts are observations, not reloads.','Write-only LCD registers are not shadowed; the menu avoids changing them.','Audio phase/FIFO contents and cartridge peripheral/save-memory protocol state are not restored.','An IRQ-return marker does not prove the BIOS return or gameplay succeeded.']
 out['structurally_valid']=not out['errors']
 return out

def main()->int:
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--json',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args()
 try:r=inspect(a.state.read_bytes())
 except OSError as e:print(f'Cannot read state: {e}',file=sys.stderr);return 2
 text=json.dumps(r,indent=2)
 if a.output:a.output.write_text(text+'\n',encoding='utf-8')
 if a.json:print(text)
 else:
  print(f"{r['format']} | {r['bytes']:#x} bytes | SHA256 {r['sha256']}")
  print('File structure: '+('PASS (not a hardware-resume result)' if r['structurally_valid'] else 'FAIL'))
  for e in r['errors']:print('  ERROR: '+e)
  if 'regions' in r:
   print(f"Game: {r['game_code']} | ABI {r['abi']} | chunks: {sum(x['matches'] for x in r['regions'])}/18")
   for x in r['regions']:print(f"  {x['index']:02d} {x['name']:<22} {'OK' if x['matches'] else 'BAD'} {x['actual_crc']}")
   d=r['diagnostic'];print(f"Diagnostic: {d['stage_name']} ({d['stage']}), detail={d['detail']}, self-CRC valid={d['self_crc_valid']}")
   print('This marker is not proof of gameplay or successful SD persistence.')
   for item in r['limitations']:print('  LIMIT: '+item)
 return 0 if r['structurally_valid'] else 1
if __name__=='__main__':raise SystemExit(main())
