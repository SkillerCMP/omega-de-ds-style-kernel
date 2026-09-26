#include <stdio.h>
#include <stdlib.h>
#include <gba_base.h>
#include <string.h>
#include <gba_dma.h>

#include "ez_define.h"
#include "draw.h"
#include "GBApatch.h"
#include "rts3_identity.h"
#include "gba_nes_patch.h"
#include "ezkernel.h"
#include "lang.h"
#include "showcht.h"
#include "Ezcard_OP.h"



u32 windows_offset;
u32 is_NORpatch;
u32 g_Offset;
u32 is_Nes;
u32 Nes_index;
u32 Nes_index_17_patch;
u32 iTrimSize;
u32 EA_offset;

u32 w_reset_on;
u32 w_rts_on;
u32 w_sleep_on;
u32 w_cheat_on;

SPatchInfo2 iPatchInfo2[EMax];
u32 iCount2;
extern ST_entry pCHEAT[];
extern u16 gl_engine_sel; /* actual definition is u16 in setwindow.c */

#define sizeofa(array) (sizeof(array)/sizeof(array[0]))

u32 spend_address;

/*
 * Save-state identity patched into each injected RTS engine.
 * The 8-byte engine/version magic lives in assembly; these final 8 bytes
 * bind the state to the current ROM's game code and byte size.
 */
static u32 g_rts_state_game_code;
static u32 g_rts_state_rom_size;
static u32 g_rts_rom_header_crc;

static void SetRtsStateIdentity(const u32 *data, u32 rom_size)
{
	g_rts_state_game_code = 0;
	g_rts_state_rom_size = rom_size;
	g_rts_rom_header_crc = (data != NULL && rom_size >= RTS3_ROM_HEADER_SIZE)
		? Rts3Crc32(data, RTS3_ROM_HEADER_SIZE) : 0;

	if (data != NULL && rom_size >= 0xB0)
		memcpy(&g_rts_state_game_code, ((const u8*)data) + 0xAC, 4);
}

static void PatchRtsStateIdentity(u8 *patchbuffer, const u8 *patch_start,
	const u8 *identity_label)
{
	u32 identity_offset = (u32)(identity_label - patch_start);
	*(vu32*)(patchbuffer + identity_offset) = g_rts_state_game_code;
	*(vu32*)(patchbuffer + identity_offset + 4) = g_rts_state_rom_size;
}

typedef struct PSRAM_ROM_CONTEXT_
{
	u32 rom_size;
} PSRAM_ROM_CONTEXT;

static u32 ReadPSRAMRomByte(u32 offset, u8 *value, void *context)
{
	PSRAM_ROM_CONTEXT *rom = (PSRAM_ROM_CONTEXT*)context;
	u32 address;
	vu16 page = 0;

	if (value == NULL || rom == NULL || offset >= rom->rom_size)
		return 0;

	address = offset;
	while (address >= 0x800000)
	{
		address -= 0x800000;
		page += 0x1000;
	}

	SetPSRampage(page);
	*value = *((vu8*)((u8*)PSRAMBase_S98 + address));
	SetPSRampage(0);
	return 1;
}

static u32 WritePSRAMRomByte(u32 offset, u8 value, void *context)
{
	PSRAM_ROM_CONTEXT *rom = (PSRAM_ROM_CONTEXT*)context;
	u32 address;
	vu16 page = 0;

	if (rom == NULL || offset >= rom->rom_size)
		return 0;

	address = offset;
	while (address >= 0x800000)
	{
		address -= 0x800000;
		page += 0x1000;
	}

	SetPSRampage(page);
	*((vu8*)((u8*)PSRAMBase_S98 + address)) = value;
	SetPSRampage(0);
	return 1;
}

//------------------------------------------------------------------
void Write(u32 romaddress, const u8* buffer, u32 size)
{
	u32 x;
	if(is_NORpatch)
	{
		if((romaddress >= windows_offset) && (romaddress < windows_offset+0x20000))
		{
			for(x=0;x<size/2;x++){
				((vu16*)(pReadCache+romaddress-windows_offset))[x] = ((vu16*)buffer)[x];
			}					
			//DEBUG_printf("NORaddress{%x}:%x %x", romaddress,size ,((vu32*)buffer)[0]);			
		}
	}
	else
	{
		u32 Address;
		vu16 page=0;
		Address=romaddress;
		while(Address>=0x800000)
		{
			Address-=0x800000;
			page+=0x1000;
		}
		SetPSRampage(page);
		
		for(x=0;x<size/2;x++)
			((vu16*)(PSRAMBase_S98 + Address))[x] = ((vu16*)buffer)[x];//todo 还要处理psram page
						
		//DEBUG_printf("address{%x}:%x %x %x %x", romaddress,page,Address,size ,((vu32*)buffer)[0]);
		SetPSRampage(0);
	}
}
//------------------------------------------------------------------
void IWRAM_CODE CheckNes(u32 *Data)
{
  u32 jump=Data[0];
  if((jump&0xff000000)==0xea000000)
  {
		Nes_index=(jump&0xffffff)+2;
		if((Data[Nes_index]&0xffffff00)==0xe28f5000&& \
		   Data[Nes_index+1]==0xe8b503d3&&\
		   Data[Nes_index+2]==0xe129f007&&\
		   Data[Nes_index+3]==0xe281deba&&\
		   Data[Nes_index+4]==0xe129f008&&\
		   Data[Nes_index+5]==0xe281debe&&\
		   Data[Nes_index+6]==0xe129f009&&\
		   Data[Nes_index+7]==0xe281dc0b&&\
		   Data[Nes_index+8]==0xe92d0003&&\
		   Data[Nes_index+9]==0xef110000&&\
		   Data[Nes_index+10]==0xe8bd8001)
		{
			if(Data[Nes_index]==0xe28f503c)
				is_Nes = 1;
			else if (Data[Nes_index]==0xe28f5040)
				is_Nes = 2;    		
		}
		else
			is_Nes = 0;
  }
}
//------------------------------------------------------------------
bool PatchNes(u32 *Data)
{
  bool res=false;
  if(is_Nes)
  {
      res=true;
      u32 patch=0xea000000|(0x3fdf5-Nes_index);
      Write(36+Nes_index*4,(u8*)&patch,sizeof(patch));
      Write(0xff800,gba_nes_patch_bin,0x340);
      if(windows_offset == 0)
    	  Nes_index_17_patch=Data[Nes_index+17+is_Nes-1];
      Write(0xff840,(u8*)&Nes_index_17_patch,sizeof(patch));
      u32 index2=(Nes_index_17_patch-0x08000000)/4; 
      if(Data[index2-1-windows_offset/4]==0x3032)
      {
        patch=0x060000f8;
        Write(0xff86c,(u8*)&patch,sizeof(patch));
      }
  }
  return res;
}
//---------------------------------------------------------------------------------
void Add2(u32 anOffset, u32 aValue)
{
	if (iCount2<(EMax))
	{
		anOffset = anOffset+ g_Offset ;
		SPatchInfo2 info = { anOffset,aValue };
		iPatchInfo2[iCount2++] = info;
	} 
}
//------------------------------------------------------------------
void IWRAM_CODE PatchInternal(u32* Data,int iSize,u32 offset)
{
  u32 search_size=iSize/4;
  g_Offset = offset/4;
  if(offset==0)
  {
	  EA_offset = Data[0] & 0xFFFFFF;
  }
  
  for(u32 ii=0;ii<search_size;ii++)
  {
    switch(Data[ii])
    {
      case 0x3007FFC: // IRQ handler
        {
          Add2(ii, 0x3007FF4);//0x3007FFC的位置
        }
        break;
      case 0x3FFFFFC: // IRQ handler
        {
          Add2(ii, 0x3007FF4);
        }
        break; 
    }
  }
}
//------------------------------------------------------------------
void SetTrimSize(u8* buffer,u32 romsize,u32 iSize,u32 mode,BYTE saveMODE)
{
  u8 byte;
  u32 bottom;
  u32 top;
  u32 alignedSize;

	u32 PATCH_LENGTH;
    if (gl_rts_on == 1 && gl_cheat_on == 0 && gl_reset_on == 0 && gl_sleep_on == 0)
        PATCH_LENGTH = (u32)((u8*)RTS_only_ReplaceIRQ_end - (u8*)RTS_only_ReplaceIRQ_start);
    else if (gl_rts_on == 1 || gl_cheat_on == 1)
        PATCH_LENGTH = (u32)((u8*)RTS_ReplaceIRQ_end - (u8*)RTS_ReplaceIRQ_start)
            + MAX_RUNTIME_CHEAT_RECORDS * 8u;
    else
        PATCH_LENGTH = (u32)((u8*)Sleep_ReplaceIRQ_end - (u8*)Sleep_ReplaceIRQ_start);
    PATCH_LENGTH = (PATCH_LENGTH + 31u) & ~15u; /* placement/alignment margin */

  if(0)
  {
	  iTrimSize = 0x1fff000;
  }
  else
  {
	  byte=buffer[(romsize-1)%iSize-1];
	  bottom=(romsize-1)%iSize-1;
	  if(bottom-16 > PATCH_LENGTH){
			top=bottom-PATCH_LENGTH-16;
		}
		else{
			top = 0;
		}
	  alignedSize=romsize+(16-(romsize&15));
	  
	  for(u32 ii=bottom;ii>=top;ii--)
	  {
			if(buffer[ii]!=byte||ii==top)
			{
			  iTrimSize=ii+4;
			  iTrimSize=iTrimSize+ ((romsize-1)&0xFFFE0000);
			  iTrimSize=iTrimSize+(16-(iTrimSize&15));
			  if(iTrimSize>alignedSize) iTrimSize=alignedSize;
			  break;
			}
	  }
  }
	//DEBUG_printf("iTrimSize %08X ", iTrimSize);
	if(mode ==1)//nor
	{
		if( ((iTrimSize&0x1FFFF)+PATCH_LENGTH)  > 0x20000)//Greater than one flash sector
		{		
			iTrimSize = ((romsize+0x1FFFF)/0x20000)*0x20000;
		}
		if(romsize <=0x1000000)
	  {
  		if((iTrimSize + PATCH_LENGTH) > 16 * 1024 * 1024)
  		{
  			if((saveMODE==0x21 )||(saveMODE==0x22 ) )//eeprom game
  			{
  				iTrimSize = 0x1000000-PATCH_LENGTH;//can not greater 16MB
  			}
  			else{//sram and flash
  				iTrimSize = 0x1000000; //page
  			}
  		}
	  }		
	}
	else
	{
	  if(romsize <=0x800000)
	  {
	  		if((iTrimSize + PATCH_LENGTH) > 8 * 1024 * 1024)
	  			iTrimSize = 0x800000; //page
	  }
	  else if(romsize <=0x1000000)
	  {
  		if((iTrimSize + PATCH_LENGTH) > 16 * 1024 * 1024)
  		{
  			if((saveMODE==0x21 )||(saveMODE==0x22 ) )//eeprom game
  			{
  				iTrimSize = 0x1000000-PATCH_LENGTH;//can not greater 16MB
  			}
  			else{//sram and flash
  				iTrimSize = 0x1000000; //page
  			}
  		}
	  }
	  else
	  {
	  		if((iTrimSize + PATCH_LENGTH) > 32 * 1024 * 1024)
	  			iTrimSize = 0x2000000-PATCH_LENGTH; //page
	  }
	}
  AutoPatch_ApplyTrimOverride();
}
//------------------------------------------------------------------
void Patch_B_address(void)
{
	if (!iCount2) return;
	u32 B_install_handler;
	B_install_handler = 0xEA000000|((iTrimSize-8)/4);
	Write(0,(u8*)&B_install_handler , 4); //B

	for (u32 ii = 0; ii<iCount2; ii++)
	{
		Write(iPatchInfo2[ii].iOffset*4, (u8*)&(iPatchInfo2[ii].iValue), sizeof(iPatchInfo2[ii].iValue));
	}	
}
//------------------------------------------------------------------
void Patch_Reset_Sleep(u32 *Data)
{	    
	Patch_B_address();
	u32 Return_address = 0x8000000+ EA_offset*4 + 8;
		
  u8 * p_patch_start  = (u8*)Sleep_ReplaceIRQ_start;
  u8 * p_patch_end 		= (u8*)Sleep_ReplaceIRQ_end;
  u8 * p_patch_Return_address_L  = (u8*)Return_address_L;

  u8* patchbuffer = (u8*)_UnusedVram ;
  u32 Return_address_offset = p_patch_Return_address_L-p_patch_start;

  dmaCopy((void*)p_patch_start,patchbuffer, p_patch_end-p_patch_start);
  *(vu32*)(patchbuffer+Return_address_offset) = Return_address;//修改gba_sleep_patch_bin里面的返回地址

	u16 read5 = Read_SET_info(assress_edit_sleephotkey_0); 
	u16 read6 = Read_SET_info(assress_edit_sleephotkey_1); 
	u16 read7 = Read_SET_info(assress_edit_sleephotkey_2); 
	u16 read8 = Read_SET_info(assress_edit_rtshotkey_0); 
	u16 read9 = Read_SET_info(assress_edit_rtshotkey_1); 
	u16 read10 = Read_SET_info(assress_edit_rtshotkey_2); 
	u16 sleep_key = ~((1<< read5) | (1<< read6) | (1<< read7));
  u16 reset_key = ~((1<< read8) | (1<< read9) | (1<< read10));	

  u32 Reset_key_offset = (u8*)Reset_key - p_patch_start;
 	u32 Sleep_key_offset = (u8*)Sleep_key - p_patch_start;

  if(gl_reset_on !=1){
  	*(vu32*)(patchbuffer+Reset_key_offset) = 0;
  }
  else{
  	*(vu32*)(patchbuffer+Reset_key_offset) = reset_key&0x3FF;
  }
  
  if(gl_sleep_on !=1){  
    *(vu32*)(patchbuffer+Sleep_key_offset) = 0;
  }
  else{
  	*(vu32*)(patchbuffer+Sleep_key_offset) = sleep_key&0x3FF;	
  }

	Write(iTrimSize, patchbuffer, p_patch_end-p_patch_start);
}
//------------------------------------------------------------------
void Patch_RTS_Cheat(u32 *Data)
{
    u32 max_runtime_size = (u32)((u8*)RTS_ReplaceIRQ_end - (u8*)RTS_ReplaceIRQ_start) + MAX_RUNTIME_CHEAT_RECORDS * 8u;
    if (max_runtime_size > 0x5400u || iTrimSize > 0x2000000u - max_runtime_size) {
        return; /* Never install a partially copied runtime. */
    }

	u32 Return_address = 0x8000000+ EA_offset*4 + 8;
	
  u8 * p_patch_start  = (u8*)RTS_ReplaceIRQ_start;
  u8 * p_patch_end  	= (u8*)RTS_ReplaceIRQ_end;
  u8 * p_patch_Return_address_L  = (u8*)RTS_Return_address_L;

  u8* patchbuffer = (u8*)_UnusedVram ;
  u32 Return_address_offset = p_patch_Return_address_L-p_patch_start;

  dmaCopy((void*)p_patch_start,patchbuffer, p_patch_end-p_patch_start);
  *(vu32*)(patchbuffer+Return_address_offset) = Return_address;//modify gba_sleep_patch_bin return address
  PatchRtsStateIdentity(patchbuffer, p_patch_start, (u8*)RTS_state_identity);
  
  if(spend_address != 0x0){
  	*(vu32*)(patchbuffer+Return_address_offset+4) = spend_address;
	}	
	
	u16 read5 = Read_SET_info(assress_edit_sleephotkey_0); 
	u16 read6 = Read_SET_info(assress_edit_sleephotkey_1); 
	u16 read7 = Read_SET_info(assress_edit_sleephotkey_2); 
	u16 read8 = Read_SET_info(assress_edit_rtshotkey_0); 
	u16 read9 = Read_SET_info(assress_edit_rtshotkey_1); 
	u16 read10 = Read_SET_info(assress_edit_rtshotkey_2); 
	u16 RTS_sleep_key_val = ~((1<< read5) | (1<< read6) | (1<< read7));
  u16 RTS_reset_key_val = ~((1<< read8) | (1<< read9) | (1<< read10));	
	 
  u32 RTS_Reset_key_offset = (u8*)RTS_Reset_key - p_patch_start;
 	u32 RTS_Sleep_key_offset = (u8*)RTS_Sleep_key - p_patch_start;
  
  *(vu32*)(patchbuffer+RTS_Reset_key_offset) = RTS_reset_key_val&0x3FF;
  if(gl_sleep_on !=1){  
    *(vu32*)(patchbuffer+RTS_Sleep_key_offset) = 0;
  }
  else{
  	*(vu32*)(patchbuffer+RTS_Sleep_key_offset) = RTS_sleep_key_val&0x3FF;	
  }

	//rts switch
	u32 rts_switch_offset = (u8*)RTS_switch - p_patch_start;
	*(vu32*)(patchbuffer+rts_switch_offset) = gl_rts_on;

	//cheat
	u8*p_no_cheat_end =  (u8*)no_CHEAT_end;
	u32 cheat_count_offset = (u8*)Cheat_count - p_patch_start;
	u32 cheat_offset = (u8*)CHEAT - p_patch_start;
	u32 output_count = 0;

	for (u32 ii = 0; ii<gl_cheat_count && output_count<MAX_RUNTIME_CHEAT_RECORDS; ii++)
	{
		if (!IsRuntimeCheatRecordActive(ii))
			continue;

		u32 record_address = pCHEAT[ii].address;
		u32 full_address;

		if (!ResolveRuntimeCheatRecordAddress(record_address, &full_address))
		{
			/* Invalid range, including runtime writes to I/O. */
			continue;
		}

		*(vu32*)(patchbuffer+cheat_offset+8*output_count) = full_address;
		*(vu32*)(patchbuffer+cheat_offset+8*output_count+4) = pCHEAT[ii].VAL;
		output_count++;
	}

	*(vu32*)(patchbuffer+cheat_count_offset) = output_count;

	u32 copysize = p_no_cheat_end-p_patch_start ;
	copysize = copysize + output_count*8;
    if (!Rts3FinalizeRuntime16(patchbuffer, copysize,
            (size_t)((u8*)RTS_rts2_header - p_patch_start),
            g_rts_state_game_code, g_rts_state_rom_size, g_rts_rom_header_crc,
            0x08000000u + iTrimSize))
        return;
    Patch_B_address();

	Write(iTrimSize, patchbuffer,copysize);
}
//------------------------------------------------------------------
void Patch_RTS_only(u32 *Data)
{
    u32 max_runtime_size = (u32)((u8*)RTS_only_ReplaceIRQ_end - (u8*)RTS_only_ReplaceIRQ_start);
    if (max_runtime_size > 0x5400u || iTrimSize > 0x2000000u - max_runtime_size) {
        return; /* Never install a partially copied runtime. */
    }

	u32 Return_address = 0x8000000+ EA_offset*4 + 8;
	
  u8 * p_patch_start  = (u8*)RTS_only_ReplaceIRQ_start;
  u8 * p_patch_end  	= (u8*)RTS_only_ReplaceIRQ_end;
  u8 * p_patch_Return_address_L  = (u8*)RTS_only_Return_address_L;

  u8* patchbuffer = (u8*)_UnusedVram ;
  u32 Return_address_offset = p_patch_Return_address_L-p_patch_start;

  dmaCopy((void*)p_patch_start,patchbuffer, p_patch_end-p_patch_start);
  *(vu32*)(patchbuffer+Return_address_offset) = Return_address;//modify gba_sleep_patch_bin return address
  PatchRtsStateIdentity(patchbuffer, p_patch_start, (u8*)RTS_only_state_identity);
  
  if(spend_address != 0x0){
  	*(vu32*)(patchbuffer+Return_address_offset+4) = spend_address;
	}	
	
	u16 read5 = Read_SET_info(assress_edit_sleephotkey_0); 
	u16 read6 = Read_SET_info(assress_edit_sleephotkey_1); 
	u16 read7 = Read_SET_info(assress_edit_sleephotkey_2); 
	u16 read8 = Read_SET_info(assress_edit_rtshotkey_0); 
	u16 read9 = Read_SET_info(assress_edit_rtshotkey_1); 
	u16 read10 = Read_SET_info(assress_edit_rtshotkey_2); 
	u16 RTS_only_SAVE_key_val = ~((1<< read5) | (1<< read6) | (1<< read7));
  u16 RTS_only_LOAD_key_val = ~((1<< read8) | (1<< read9) | (1<< read10));	
	 
  u32 RTS_only_SAVE_key_offset = (u8*)RTS_only_SAVE_key - p_patch_start;
 	u32 RTS_only_LOAD_key_offset = (u8*)RTS_only_LOAD_key - p_patch_start;
  
  *(vu32*)(patchbuffer+RTS_only_SAVE_key_offset) = RTS_only_SAVE_key_val&0x3FF;
  *(vu32*)(patchbuffer+RTS_only_LOAD_key_offset) = RTS_only_LOAD_key_val&0x3FF;	

	u32 copysize = p_patch_end - p_patch_start ;
	
    if (!Rts3FinalizeRuntime16(patchbuffer, copysize,
            (size_t)((u8*)RTS_only_rts2_header - p_patch_start),
            g_rts_state_game_code, g_rts_state_rom_size, g_rts_rom_header_crc,
            0x08000000u + iTrimSize))
        return;
    Patch_B_address();

	Write(iTrimSize, patchbuffer,copysize);
}
//------------------------------------------------------------------
void GBApatch_Cleanrom(u32* address,int filesize)//Only once
{
	windows_offset = 0;
	is_NORpatch = 0;
	CheckNes(address);
	PatchNes(address);
	AutoPatch_ApplyFixedWrites();
}
//------------------------------------------------------------------
u32 Get_spend_address(u32* Data)
{
	u32 ii;	
	u32 offset;
	u32 updown;
	
  u32 search_size=0x5000/4; 
  for(ii=0;ii<search_size;ii++)
  {
		u32 word1 = Data[ii] & 0xFFFF001F;
		u32 word2 = Data[ii+1];
		if (word1 == 0xE3A0001F)
		{
			if ((word2 == 0xE129F000) || (word2 == 0xE121F000) )
			{
				offset= Data[ii + 2] & 0xFFF;
				updown = Data[ii + 2] & 0x00F00000;
				break;
			}
		}
  }
  if(ii == search_size) 
  {
  	return 0;//Find_spend_address_SpecialROM(Data); 
  }
  
  u32 address;
  if(updown == 0x00900000)
		address = ii*4 + offset+16;
	else if (updown == 0x00100000)
		address = ii*4 - offset+16;
	else
		address = 0;
  
  if(	(Data[address/4] > 0x03007E80) /*|| (Data[address/4] == 0x03007E00)*/ || (Data[address/4] == 0x0203FFFC) )
  {
  	Data[address/4] = Data[address/4] - 0x80; 
  	return (Data[address/4]);
  }
  else 
  	return 0;	
}
//------------------------------------------------------------------
void GBApatch_PSRAM(u32* address,int filesize)//Only once
{
	PSRAM_ROM_CONTEXT rom_context;

	windows_offset = 0;
	is_NORpatch = 0;
	EA_offset = address[0] & 0xFFFFFF;
	rom_context.rom_size = (u32)filesize;
	SetRtsStateIdentity(address, (u32)filesize);

	/* ROMIF: checks the original loaded image before any kernel patches. */
	if (gl_cheat_on == 1)
		EvaluateEnhancedCheatGroups(ReadPSRAMRomByte, &rom_context,
			rom_context.rom_size);
	
	CheckNes(address);
	PatchNes(address);
	AutoPatch_ApplyFixedWrites();
	AutoPatch_ApplySearchWrites(address);

	/* ROM: bytes are final boot-time image edits and use no IRQ slots. */
	if (gl_cheat_on == 1)
		ApplyActiveRomPatches(WritePSRAMRomByte, &rom_context,
			rom_context.rom_size);
	
	if( (gl_rts_on==1) && (gl_cheat_on == 0)  && (gl_reset_on == 0)  && (gl_sleep_on == 0)  ) {
		spend_address = 0; /* 13.7e1: CPU backup uses reserved SRAM, not game RAM. */
		Patch_RTS_only(address);		
	}
	else if((gl_rts_on==1) ||  ((gl_cheat_on==1)&& (gl_cheat_count>0) ) )		
	{
		spend_address = 0; /* 13.7e1: CPU backup uses reserved SRAM, not game RAM. */
		//DEBUG_printf("spend_address =%x",spend_address);
		Patch_RTS_Cheat(address);
	}
	else
	{  
		Patch_Reset_Sleep(address);
	}
}
//------------------------------------------------------------------
void GBApatch_Cleanrom_NOR(u32* address,u32 offset)
{
	windows_offset = offset;
	is_NORpatch = 1;
	if(offset==0)
  {
		CheckNes(address);
	}
	PatchNes(address);
	AutoPatch_ApplyFixedWrites();
}
//------------------------------------------------------------------
void GBApatch_NOR(u32* address,int filesize,u32 offset)
{
    /* NOR writes its entry hook in block zero, before Patch_RTS_* runs.
     * Apply the same capacity gate here so a failure cannot leave that hook
     * pointing to an absent/truncated runtime. */
    if (gl_rts_on == 1 || (gl_cheat_on == 1 && gl_cheat_count > 0)) {
        u32 bytes;
        if (gl_rts_on == 1 && gl_cheat_on == 0 && gl_reset_on == 0 && gl_sleep_on == 0)
            bytes=(u32)((u8*)RTS_only_ReplaceIRQ_end-(u8*)RTS_only_ReplaceIRQ_start);
        else
            bytes=(u32)((u8*)RTS_ReplaceIRQ_end-(u8*)RTS_ReplaceIRQ_start)
                + MAX_RUNTIME_CHEAT_RECORDS*8u;
        if (bytes > 0x5400u || iTrimSize > 0x2000000u-bytes)
            return;
    }

	windows_offset = offset;
	is_NORpatch = 1;
  if(offset==0)
  {
	  CheckNes(address);
	  EA_offset = address[0] & 0xFFFFFF;
		  SetRtsStateIdentity(address, (u32)filesize);
	  
		u32 B_install_handler;
		B_install_handler = 0xEA000000|((iTrimSize-8)/4);
		Write(0,(u8*)&B_install_handler , 4); //B
		spend_address = 0; /* 13.7e1: CPU backup uses reserved SRAM, not game RAM. */
		
		AutoPatch_ApplySearchWrites(address);
  }
	PatchNes(address);
	AutoPatch_ApplyFixedWrites();

	if( (gl_rts_on==1) && (gl_cheat_on == 0)  && (gl_reset_on == 0)  && (gl_sleep_on == 0)  ) {
		Patch_RTS_only(address);		
	}
	else if((gl_rts_on==1) ||  ((gl_cheat_on==1)&& (gl_cheat_count>0) ) )		
	{
		Patch_RTS_Cheat(address);
	} 
	else
	{  
		Patch_Reset_Sleep(address);
	}
}
//------------------------------------------------------------------
void make_pat_name(TCHAR*patnamebuf,TCHAR* gamefilename)
{
	memcpy(patnamebuf,gamefilename,100);
	u32 len=strlen(patnamebuf);
	patnamebuf[len-3] = 'p';
	patnamebuf[len-2] = 'a';
	patnamebuf[len-1] = 't';	
}
//------------------------------------------------------------------
void GBA_patch_init(void)
{
	is_NORpatch = 0;
	windows_offset = 0;
	is_Nes = 0;
	Nes_index = 0;
	g_Offset = 0;

	iCount2 = 0;
	iTrimSize = 0;
	EA_offset = 0;
	
	w_reset_on = 0;
	w_rts_on = 0;
	w_sleep_on = 0;
	w_cheat_on = 0;

	memset(iPatchInfo2, 0x00, sizeof(iPatchInfo2));
}
//------------------------------------------------------------------
void GBA_patch_init_buffer(u32* buffer)
{
	memcpy(iPatchInfo2, buffer, sizeof(iPatchInfo2));
	u32 start = sizeof(iPatchInfo2)/4;
	 
	is_NORpatch = buffer[start+0];
	windows_offset = buffer[start+1];
	is_Nes = buffer[start+2];
	Nes_index = buffer[start+3];
	g_Offset = buffer[start+4];
	iCount2 = buffer[start+5];
	iTrimSize = buffer[start+6];
	EA_offset = buffer[start+7];
	
	w_reset_on = buffer[start+8];
	w_rts_on = buffer[start+9];
	w_sleep_on = buffer[start+10];
	w_cheat_on = buffer[start+11];
}
//------------------------------------------------------------------
u32 Check_pat(TCHAR* gamefilename)
{
    /* Table/external mode must consult today's external variants before any
     * old cache. BPRE/BPGE are also rebuilt in scanner mode after this change. */
    if ((gl_engine_sel != 0 && gl_select_lang != 0xE2E2) ||
        !memcmp(GAMECODE,"BPRE",4) || !memcmp(GAMECODE,"BPGE",4)) {
        GBA_patch_init();
        return 0;
    }

	UINT  ret;
	u32 find_the_patfile;
	u32 patfilesize;
	u32 res;
	
	TCHAR patnamebuf[100];	
	make_pat_name(patnamebuf,gamefilename);
	res=f_chdir("/SYSTEM/PATCH");
	if(res == FR_OK)
	{
		res = f_open(&gfile,patnamebuf, FA_READ);
		
		if(res == FR_OK)//have a old file
		{
			patfilesize = f_size(&gfile);
			ret = 0;
            find_the_patfile = 0;
            if (patfilesize == sizeof(iPatchInfo2) + 16u * sizeof(u32)) {
                res = f_read(&gfile, pReadCache, patfilesize, &ret);
                if (res == FR_OK && ret == patfilesize) {
                    const u32 *cache = (const u32*)pReadCache;
                    u32 trailer = sizeof(iPatchInfo2) / (sizeof(u32));
                    if (cache[trailer + 12] == RTS3_KERNEL_ABI &&
                        cache[trailer + 5] <= EMax && cache[trailer + 15] == 0 &&
                        cache[trailer + 13] == Rts3Crc32(pReadCache,sizeof(iPatchInfo2)) &&
                        cache[trailer + 14] == Rts3Crc32(cache + trailer,14u*sizeof(u32)))
                        find_the_patfile = 1;
                }
            }
            f_close(&gfile);
		}					
		else
		{
			find_the_patfile = 0;
		}
		//res=f_chdir("/");
	}
	else//no PATCH folder
	{
		find_the_patfile = 0;
	}

	if(find_the_patfile)
	{
		//read patch information
		GBA_patch_init_buffer((u32*)pReadCache);
		
		if( (w_reset_on !=gl_reset_on)  || (w_rts_on !=gl_rts_on)  || (w_sleep_on !=gl_sleep_on) || (w_cheat_on !=gl_cheat_on))
		{
			GBA_patch_init();
			find_the_patfile = 0;
		}	
	}
	else
	{
		GBA_patch_init();
	}	
	return find_the_patfile;
}
//------------------------------------------------------------------
void Make_pat_file(TCHAR* gamefilename)
{
	u32 res;
	u32 written;
	u32 w_buffer[16];
	
	res = f_mkdir("/SYSTEM/PATCH");
	res=f_chdir("/SYSTEM/PATCH");
	
	memset(w_buffer, 0x00, sizeof(w_buffer));

	if(res == FR_OK){
		TCHAR patnamebuf[100];	
		make_pat_name(patnamebuf,gamefilename);

		res = f_open(&gfile,patnamebuf, FA_WRITE | FA_CREATE_ALWAYS);
		if(res == FR_OK)
		{	
			f_lseek(&gfile, 0x0000);
			res=f_write(&gfile, (void*)iPatchInfo2, sizeof(iPatchInfo2), (UINT*)&written);
			w_buffer[0] = is_NORpatch;
			w_buffer[1] = windows_offset;
			w_buffer[2] = is_Nes;
			w_buffer[3] = Nes_index;
			w_buffer[4] = g_Offset;
			w_buffer[5] = iCount2;
			w_buffer[6] = iTrimSize;
			w_buffer[7] = EA_offset;

			w_buffer[8] = gl_reset_on;
			w_buffer[9] = gl_rts_on;
			w_buffer[10] = gl_sleep_on;
			w_buffer[11] = gl_cheat_on;
            w_buffer[12] = RTS3_KERNEL_ABI; /* Reject stale runtime placement caches. */
            w_buffer[13] = Rts3Crc32(iPatchInfo2,sizeof(iPatchInfo2));
            w_buffer[14] = Rts3Crc32(w_buffer,14u*sizeof(u32));
					
			res=f_write(&gfile, (void*)w_buffer, sizeof(w_buffer), (UINT*)&written);
			f_close(&gfile);
		}
	}
	res=f_chdir("/");
}
//------------------------------------------------------------------
void make_mde_name(TCHAR*mdenamebuf,TCHAR* gamefilename)
{
	memcpy(mdenamebuf,gamefilename,100);
	u32 len=strlen(mdenamebuf);
	mdenamebuf[len-3] = 'm';
	mdenamebuf[len-2] = 'd';
	mdenamebuf[len-1] = 'e';	
}
//------------------------------------------------------------------
u8 Check_mde_file(TCHAR* gamefilename)
{
	UINT  ret;
	u32 find_the_mdefile;
	u32 mdefilesize;
	u32 res;
	
	TCHAR mdenamebuf[100];	
	make_mde_name(mdenamebuf,gamefilename);
	
	res=f_chdir(SAVER_FOLDER);
	if(res == FR_OK)
	{
		res = f_open(&gfile,mdenamebuf, FA_OPEN_EXISTING);
		
		if(res == FR_OK)//have a old file
		{
			f_open(&gfile,mdenamebuf, FA_READ);
			mdefilesize = f_size(&gfile);
			f_read(&gfile, pReadCache, mdefilesize, &ret);
			f_close(&gfile);
			find_the_mdefile = 1;
		}					
		else
		{
			find_the_mdefile = 0;
		}
	}
	else//cant fine folder
	{
		find_the_mdefile = 0;
	}

	if(find_the_mdefile)
	{
		//read
		return (pReadCache[0]);
	}
	else
	{
		return 0;
	}	
}
//------------------------------------------------------------------
u8 Make_mde_file(TCHAR* gamefilename,u8 Save_num)
{
	u32 res;
	u32 written;
	u8 w_buffer[16];

	TCHAR currentpath[256];
	memset(currentpath,00,256);
	res = f_getcwd(currentpath, sizeof currentpath / sizeof *currentpath);

	//res = f_mkdir(SAVER_FOLDER);
	res=f_chdir(SAVER_FOLDER);
	if(res != FR_OK){
			return 2;
	}

	memset(w_buffer, 0x00, sizeof(w_buffer));

	if(res == FR_OK){
		TCHAR mdenamebuf[100];
		make_mde_name(mdenamebuf,gamefilename);

		res = f_open(&gfile,mdenamebuf, FA_WRITE | FA_OPEN_ALWAYS);
		if(res == FR_OK)
		{
			f_lseek(&gfile, 0x0000);

			w_buffer[0] = Save_num;
			res=f_write(&gfile, (void*)w_buffer, sizeof(w_buffer), (UINT*)&written);

			f_close(&gfile);
		}
	}
	res=f_chdir(currentpath);
	return res;
}
//------------------------------------------------------------------
#define RTS_FILE_SIZE 0x70000u
#define RTS_CREATE_CHUNK 0x800u

static u32 CreateRtsFile(const TCHAR *filename)
{
	u32 offset;
	UINT written;
	FRESULT res;

	res = f_open(&gfile, filename, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK)
		return 0;

	memset(pReadCache, 0xFF, RTS_CREATE_CHUNK);
	for (offset = 0; offset < RTS_FILE_SIZE; offset += RTS_CREATE_CHUNK)
	{
		written = 0;
		res = f_write(&gfile, pReadCache, RTS_CREATE_CHUNK, &written);
		if (res != FR_OK || written != RTS_CREATE_CHUNK)
		{
			f_close(&gfile);
			return 0;
		}
	}

	res = f_sync(&gfile);
	if (res != FR_OK || f_size(&gfile) != RTS_FILE_SIZE)
	{
		f_close(&gfile);
		return 0;
	}

	return f_close(&gfile) == FR_OK;
}

u32 Check_RTS(TCHAR* gamefilename)
{
	u32 res;
	u32 rtsfilesize = 0;
	TCHAR rtsnamebuf[100];

	memcpy(rtsnamebuf, gamefilename, sizeof(rtsnamebuf));
	rtsnamebuf[sizeof(rtsnamebuf) - 1] = 0;
	{
		u32 len = strlen(rtsnamebuf);
		if (len < 3)
			return 0;
		rtsnamebuf[len-3] = 'r';
		rtsnamebuf[len-2] = 't';
		rtsnamebuf[len-1] = 's';
	}

	f_mkdir("/SYSTEM/RTS");
	res = f_chdir("/SYSTEM/RTS");
	if (res != FR_OK)
		return 0;

	res = f_open(&gfile, rtsnamebuf, FA_OPEN_EXISTING);
	if (res == FR_OK)
	{
		rtsfilesize = f_size(&gfile);
		f_close(&gfile);
	}

	if (rtsfilesize != RTS_FILE_SIZE)
	{
		ShowbootProgress(gl_make_RTS);
		if (!CreateRtsFile(rtsnamebuf))
			return 0;
		rtsfilesize = RTS_FILE_SIZE;
	}

	if (!LoadRTSfile(rtsnamebuf))
		return 0;

	res = Check_game_RTS_FAT(rtsnamebuf, 3);
	if (res == 0xffffffff)
		return 0;

	return rtsfilesize;
}
//------------------------------------------------------------------
#define AUTO_PATCH_EXTERNAL_MISSING 0u
#define AUTO_PATCH_EXTERNAL_MATCHED 1u
#define AUTO_PATCH_EXTERNAL_NO_MATCH 2u
#define AUTO_PATCH_FORMAT_VERSION 2u
#define AUTO_PATCH_MAX_VARIANT_IRQ 96u
#define AUTO_PATCH_MAX_ADD32 8u
#define AUTO_PATCH_MAX_PATCH16 24u
#define AUTO_PATCH_MAX_PATCH32 8u
#define AUTO_PATCH_MAX_SEARCH32PAIR 4u

typedef struct AUTO_PATCH_WRITE32_
{
	u32 offset;
	u32 value;
} AUTO_PATCH_WRITE32;

typedef struct AUTO_PATCH_WRITE16_
{
	u32 offset;
	u16 value;
	u16 reserved;
} AUTO_PATCH_WRITE16;

typedef struct AUTO_PATCH_SEARCH32PAIR_
{
	u32 start;
	u32 length;
	u32 word0;
	u32 word1;
	u32 write_delta;
	u32 replacement;
} AUTO_PATCH_SEARCH32PAIR;

typedef struct AUTO_PATCH_PROFILE_
{
	u32 game_code;
	u32 variant;
	u32 no_irq;
	u32 irq_total;
	u32 irq_count;
	u32 irq_offsets[EMax];
	u32 add32_count;
	AUTO_PATCH_WRITE32 add32[AUTO_PATCH_MAX_ADD32];
	u32 patch16_count;
	AUTO_PATCH_WRITE16 patch16[AUTO_PATCH_MAX_PATCH16];
	u32 patch32_count;
	AUTO_PATCH_WRITE32 patch32[AUTO_PATCH_MAX_PATCH32];
	u32 trim_valid;
	u32 trim_size;
	u32 search_count;
	AUTO_PATCH_SEARCH32PAIR search[AUTO_PATCH_MAX_SEARCH32PAIR];
} AUTO_PATCH_PROFILE;

static AUTO_PATCH_PROFILE g_auto_patch_profile;
static u32 g_auto_patch_status = AUTO_PATCH_EXTERNAL_MISSING;
static u32 g_auto_patch_prepared_game_code = 0;
static u32 g_auto_patch_prepared_rom_size = 0;

static u32 AutoPatch_IsSafeGameCode(const u8 gamecode[4])
{
	u32 index;
	for(index = 0; index < 4; index++)
	{
		if(!((gamecode[index] >= '0' && gamecode[index] <= '9') ||
			(gamecode[index] >= 'A' && gamecode[index] <= 'Z') ||
			(gamecode[index] >= 'a' && gamecode[index] <= 'z')))
			return 0;
	}
	return 1;
}

static char AutoPatch_HexDigit(u8 value)
{
	value &= 0x0F;
	return (char)((value < 10) ? ('0' + value) : ('A' + value - 10));
}

static void AutoPatch_BuildExternalPath(char *path, u32 path_size, const u8 gamecode[4])
{
	if(path == NULL || path_size == 0)
		return;
	path[0] = 0;

	if(AutoPatch_IsSafeGameCode(gamecode))
	{
		snprintf(path, path_size, "/SYSTEM/PATCHES/GBA/%c/%c/%c%c%c%c.patch",
			gamecode[0], gamecode[1], gamecode[0], gamecode[1], gamecode[2], gamecode[3]);
	}
	else if(path_size >= sizeof("/SYSTEM/PATCHES/GBA/_HEX/00000000.patch"))
	{
		char *out = path;
		const char prefix[] = "/SYSTEM/PATCHES/GBA/_HEX/";
		u32 index;
		memcpy(out, prefix, sizeof(prefix) - 1);
		out += sizeof(prefix) - 1;
		for(index = 0; index < 4; index++)
		{
			*out++ = AutoPatch_HexDigit(gamecode[index] >> 4);
			*out++ = AutoPatch_HexDigit(gamecode[index]);
		}
		memcpy(out, ".patch", sizeof(".patch"));
	}
}

static u32 AutoPatch_ParseHex32(const char *text, u32 *value)
{
	u32 result = 0;
	u32 digits = 0;
	if(text == NULL || value == NULL)
		return 0;
	while(*text)
	{
		u32 nibble;
		char ch = *text++;
		if(ch >= '0' && ch <= '9') nibble = (u32)(ch - '0');
		else if(ch >= 'A' && ch <= 'F') nibble = (u32)(ch - 'A' + 10);
		else if(ch >= 'a' && ch <= 'f') nibble = (u32)(ch - 'a' + 10);
		else return 0;
		if(digits >= 8) return 0;
		result = (result << 4) | nibble;
		digits++;
	}
	if(digits == 0)
		return 0;
	*value = result;
	return 1;
}

static u32 AutoPatch_ParseHexFields(const char *text, u32 *values, u32 count)
{
	u32 index;
	const char *cursor = text;
	if(text == NULL || values == NULL || count == 0)
		return 0;
	for(index = 0; index < count; index++)
	{
		char temp[9];
		u32 length = 0;
		while(cursor[length] && cursor[length] != ',')
		{
			if(length >= 8) return 0;
			temp[length] = cursor[length];
			length++;
		}
		if(length == 0) return 0;
		temp[length] = 0;
		if(!AutoPatch_ParseHex32(temp, &values[index])) return 0;
		cursor += length;
		if(index + 1 < count)
		{
			if(*cursor != ',') return 0;
			cursor++;
		}
		else if(*cursor != 0)
			return 0;
	}
	return 1;
}

static u32 AutoPatch_GameLineMatches(const char *line, const u8 gamecode[4])
{
	if(!strncmp(line, "GAME=", 5))
		return strlen(line + 5) == 4 && !memcmp(line + 5, gamecode, 4);

	if(!strncmp(line, "GAMEHEX=", 8))
	{
		u32 index;
		const char *hex = line + 8;
		if(strlen(hex) != 8)
			return 0;
		for(index = 0; index < 4; index++)
		{
			u32 pair;
			char temp[3];
			temp[0] = hex[index * 2];
			temp[1] = hex[index * 2 + 1];
			temp[2] = 0;
			if(!AutoPatch_ParseHex32(temp, &pair) || pair != gamecode[index])
				return 0;
		}
		return 1;
	}
	return 0;
}

static u32 AutoPatch_VerifyIrqVariant(FIL *rom, const u32 *offsets, u32 count, u32 romsize)
{
	u32 index;
	UINT read = 0;
	if(rom == NULL || offsets == NULL || count == 0)
		return 0;
	for(index = 0; index < count; index++)
	{
		u32 value = 0;
		u32 offset = offsets[index];
		if((offset & 3u) != 0 || offset > romsize || romsize - offset < 4u)
			return 0;
		if(f_lseek(rom, offset) != FR_OK)
			return 0;
		read = 0;
		if(f_read(rom, &value, sizeof(value), &read) != FR_OK || read != sizeof(value))
			return 0;
		if(value != 0x03007FFCu && value != 0x03FFFFFCu)
			return 0;
	}
	return 1;
}

u32 AutoPatch_PrepareProfile(TCHAR* gamefilename, u8 gamecode[], u32 romsize)
{
	char patch_path[64];
	u32 patch_size;
	UINT read = 0;
	FRESULT result;
	char *cursor;
	char *end;
	u32 format_ok = 0;
	u32 game_ok = 0;
	u32 in_variant = 0;
	u32 variant_number = 0;
	u32 irq_seen = 0;
	AUTO_PATCH_PROFILE *candidate = (AUTO_PATCH_PROFILE*)(pReadCache + MAX_pReadCache_size - sizeof(AUTO_PATCH_PROFILE));

	memset(&g_auto_patch_profile, 0, sizeof(g_auto_patch_profile));
	g_auto_patch_status = AUTO_PATCH_EXTERNAL_MISSING;
	g_auto_patch_prepared_game_code = 0;
	g_auto_patch_prepared_rom_size = romsize;
	if(gamecode != NULL)
		memcpy(&g_auto_patch_prepared_game_code, gamecode, 4);

	if(gamefilename == NULL || gamecode == NULL || romsize < 4)
		return g_auto_patch_status;

	AutoPatch_BuildExternalPath(patch_path, sizeof(patch_path), gamecode);
	if(patch_path[0] == 0)
		return g_auto_patch_status;

	result = f_open(&gfile, patch_path, FA_READ);
	if(result != FR_OK)
		return g_auto_patch_status;

	patch_size = f_size(&gfile);
	if(patch_size == 0 || patch_size + 1u >= MAX_pReadCache_size - sizeof(AUTO_PATCH_PROFILE))
	{
		f_close(&gfile);
		g_auto_patch_status = AUTO_PATCH_EXTERNAL_NO_MATCH;
		return g_auto_patch_status;
	}
	result = f_read(&gfile, pReadCache, patch_size, &read);
	f_close(&gfile);
	if(result != FR_OK || read != patch_size)
	{
		g_auto_patch_status = AUTO_PATCH_EXTERNAL_NO_MATCH;
		return g_auto_patch_status;
	}
	pReadCache[patch_size] = 0;

	result = f_open(&gfile, gamefilename, FA_READ);
	if(result != FR_OK)
	{
		g_auto_patch_status = AUTO_PATCH_EXTERNAL_NO_MATCH;
		return g_auto_patch_status;
	}

	memset(candidate, 0, sizeof(*candidate));
	cursor = (char*)pReadCache;
	end = cursor + patch_size;
	while(cursor < end)
	{
		char *line = cursor;
		char *line_end;
		while(cursor < end && *cursor != '\n' && *cursor != '\r') cursor++;
		line_end = cursor;
		while(cursor < end && (*cursor == '\n' || *cursor == '\r')) cursor++;
		*line_end = 0;
		while(*line == ' ' || *line == '\t') line++;
		while(line_end > line && (line_end[-1] == ' ' || line_end[-1] == '\t')) *--line_end = 0;
		if(*line == 0 || *line == '#')
			continue;

		if(!strncmp(line, "FORMAT=", 7))
		{
			u32 version;
			if(!AutoPatch_ParseHex32(line + 7, &version) || version != AUTO_PATCH_FORMAT_VERSION)
				goto external_invalid;
			format_ok = 1;
			continue;
		}
		if(!strncmp(line, "GAME=", 5) || !strncmp(line, "GAMEHEX=", 8))
		{
			if(!AutoPatch_GameLineMatches(line, gamecode))
				goto external_invalid;
			game_ok = 1;
			continue;
		}
		if(!strncmp(line, "VARIANT=", 8))
		{
			u32 parsed_variant;
			if(in_variant || !format_ok || !game_ok ||
				!AutoPatch_ParseHex32(line + 8, &parsed_variant))
				goto external_invalid;
			in_variant = 1;
			variant_number = parsed_variant;
			irq_seen = 0;
			memset(candidate, 0, sizeof(*candidate));
			candidate->variant = variant_number;
			memcpy(&candidate->game_code, gamecode, 4);
			continue;
		}
		if(!strcmp(line, "NO_IRQ=1"))
		{
			if(!in_variant || irq_seen != 0)
				goto external_invalid;
			candidate->no_irq = 1;
			continue;
		}
		if(!strncmp(line, "IRQ32=", 6))
		{
			u32 offset;
			if(!in_variant || candidate->no_irq || irq_seen >= AUTO_PATCH_MAX_VARIANT_IRQ ||
				!AutoPatch_ParseHex32(line + 6, &offset))
				goto external_invalid;
			if(candidate->irq_count < EMax)
				candidate->irq_offsets[candidate->irq_count++] = offset;
			irq_seen++;
			candidate->irq_total = irq_seen;
			continue;
		}
		if(!strncmp(line, "ADD32=", 6))
		{
			u32 fields[2];
			if(!in_variant || candidate->add32_count >= AUTO_PATCH_MAX_ADD32 ||
				!AutoPatch_ParseHexFields(line + 6, fields, 2) || (fields[0] & 3u) != 0)
				goto external_invalid;
			candidate->add32[candidate->add32_count].offset = fields[0];
			candidate->add32[candidate->add32_count].value = fields[1];
			candidate->add32_count++;
			continue;
		}
		if(!strncmp(line, "PATCH16=", 8))
		{
			u32 fields[2];
			if(!in_variant || candidate->patch16_count >= AUTO_PATCH_MAX_PATCH16 ||
				!AutoPatch_ParseHexFields(line + 8, fields, 2) || fields[1] > 0xFFFFu ||
				(fields[0] & 1u) != 0 || fields[0] >= romsize || romsize - fields[0] < 2u)
				goto external_invalid;
			candidate->patch16[candidate->patch16_count].offset = fields[0];
			candidate->patch16[candidate->patch16_count].value = (u16)fields[1];
			candidate->patch16_count++;
			continue;
		}
		if(!strncmp(line, "PATCH32=", 8))
		{
			u32 fields[2];
			if(!in_variant || candidate->patch32_count >= AUTO_PATCH_MAX_PATCH32 ||
				!AutoPatch_ParseHexFields(line + 8, fields, 2) || (fields[0] & 3u) != 0 ||
				fields[0] >= romsize || romsize - fields[0] < 4u)
				goto external_invalid;
			candidate->patch32[candidate->patch32_count].offset = fields[0];
			candidate->patch32[candidate->patch32_count].value = fields[1];
			candidate->patch32_count++;
			continue;
		}
		if(!strncmp(line, "TRIM=", 5))
		{
			u32 value;
			if(!in_variant || candidate->trim_valid || !AutoPatch_ParseHex32(line + 5, &value) || value > romsize)
				goto external_invalid;
			candidate->trim_valid = 1;
			candidate->trim_size = value;
			continue;
		}
		if(!strncmp(line, "SEARCH32PAIR=", 13))
		{
			u32 fields[6];
			AUTO_PATCH_SEARCH32PAIR *search;
			if(!in_variant || candidate->search_count >= AUTO_PATCH_MAX_SEARCH32PAIR ||
				!AutoPatch_ParseHexFields(line + 13, fields, 6))
				goto external_invalid;
			if((fields[0] & 3u) || (fields[1] & 3u) || fields[1] == 0 ||
				(fields[4] & 3u) || fields[0] >= romsize || fields[1] > romsize - fields[0])
				goto external_invalid;
			search = &candidate->search[candidate->search_count++];
			search->start = fields[0]; search->length = fields[1];
			search->word0 = fields[2]; search->word1 = fields[3];
			search->write_delta = fields[4]; search->replacement = fields[5];
			continue;
		}
		if(!strcmp(line, "END"))
		{
			if(!in_variant || (irq_seen == 0 && !candidate->no_irq))
				goto external_invalid;
			if(candidate->no_irq || AutoPatch_VerifyIrqVariant(&gfile, candidate->irq_offsets,
				candidate->irq_count, romsize))
			{
				f_close(&gfile);
				memcpy(&g_auto_patch_profile, candidate, sizeof(g_auto_patch_profile));
				g_auto_patch_status = AUTO_PATCH_EXTERNAL_MATCHED;
				return g_auto_patch_status;
			}
			in_variant = 0;
			variant_number = 0;
			irq_seen = 0;
			memset(candidate, 0, sizeof(*candidate));
			continue;
		}
		goto external_invalid;
	}

	f_close(&gfile);
	g_auto_patch_status = AUTO_PATCH_EXTERNAL_NO_MATCH;
	return g_auto_patch_status;

external_invalid:
	f_close(&gfile);
	memset(&g_auto_patch_profile, 0, sizeof(g_auto_patch_profile));
	g_auto_patch_status = AUTO_PATCH_EXTERNAL_NO_MATCH;
	return g_auto_patch_status;
}

u32 use_external_patch_engine(TCHAR* gamefilename, u8 gamecode[], u32 romsize)
{
	u32 requested_game_code = 0;
	u32 index;
	if(gamecode == NULL)
		return AUTO_PATCH_EXTERNAL_MISSING;
	memcpy(&requested_game_code, gamecode, 4);
	if(g_auto_patch_prepared_game_code != requested_game_code || g_auto_patch_prepared_rom_size != romsize)
		AutoPatch_PrepareProfile(gamefilename, gamecode, romsize);
	if(g_auto_patch_status != AUTO_PATCH_EXTERNAL_MATCHED)
		return g_auto_patch_status;

	g_Offset = 0;
	iCount2 = 0;
	for(index = 0; index < g_auto_patch_profile.irq_count && index < EMax; index++)
		Add2(g_auto_patch_profile.irq_offsets[index] / 4u, 0x03007FF4u);
	return AUTO_PATCH_EXTERNAL_MATCHED;
}

void AutoPatch_AppendDeferredRecords(void)
{
	u32 index;
	if(g_auto_patch_status != AUTO_PATCH_EXTERNAL_MATCHED)
		return;
	for(index = 0; index < g_auto_patch_profile.add32_count; index++)
		Add2(g_auto_patch_profile.add32[index].offset / 4u, g_auto_patch_profile.add32[index].value);
}

void AutoPatch_ApplyTrimOverride(void)
{
	if(g_auto_patch_status == AUTO_PATCH_EXTERNAL_MATCHED && g_auto_patch_profile.trim_valid)
		iTrimSize = g_auto_patch_profile.trim_size;
}

void AutoPatch_ApplyFixedWrites(void)
{
	u32 index;
	if(g_auto_patch_status != AUTO_PATCH_EXTERNAL_MATCHED)
		return;
	for(index = 0; index < g_auto_patch_profile.patch16_count; index++)
	{
		u16 value = g_auto_patch_profile.patch16[index].value;
		Write(g_auto_patch_profile.patch16[index].offset, (const u8*)&value, sizeof(value));
	}
	for(index = 0; index < g_auto_patch_profile.patch32_count; index++)
	{
		u32 value = g_auto_patch_profile.patch32[index].value;
		Write(g_auto_patch_profile.patch32[index].offset, (const u8*)&value, sizeof(value));
	}
}

void AutoPatch_ApplySearchWrites(u32 *Data)
{
	u32 search_index;
	if(g_auto_patch_status != AUTO_PATCH_EXTERNAL_MATCHED || Data == NULL)
		return;
	for(search_index = 0; search_index < g_auto_patch_profile.search_count; search_index++)
	{
		const AUTO_PATCH_SEARCH32PAIR *search = &g_auto_patch_profile.search[search_index];
		u32 position;
		for(position = search->start; position < search->start + search->length; position += 4u)
		{
			if(Data[position / 4u] == search->word0 && Data[position / 4u + 1u] == search->word1)
			{
				u32 value = search->replacement;
				Write(position + search->write_delta, (const u8*)&value, sizeof(value));
			}
		}
	}
}

