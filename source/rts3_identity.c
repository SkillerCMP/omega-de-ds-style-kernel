#include "rts3_identity.h"
static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
uint32_t Rts3Crc32(const void *data, size_t size)
{
    static const uint32_t table[16] = {
        0x00000000u,0x1DB71064u,0x3B6E20C8u,0x26D930ACu,
        0x76DC4190u,0x6B6B51F4u,0x4DB26158u,0x5005713Cu,
        0xEDB88320u,0xF00F9344u,0xD6D6A3E8u,0xCB61B38Cu,
        0x9B64C2B0u,0x86D3D2D4u,0xA00AE278u,0xBDBDF21Cu };
    const uint8_t *p=(const uint8_t *)data;
    uint32_t crc=UINT32_C(0xFFFFFFFF);
    while (size-- != 0u) {
        crc ^= *p++;
        crc=(crc>>4)^table[crc&15u];
        crc=(crc>>4)^table[crc&15u];
    }
    return ~crc;
}
static int finalize_runtime(uint8_t *runtime, size_t runtime_size,
    size_t header_offset, uint32_t game_code, uint32_t rom_size,
    uint32_t rom_header_crc, uint32_t installed_address,
    void (*store32)(uint8_t *, uint32_t))
{
    uint8_t *h;
    uint32_t crc;
    if (runtime == NULL || runtime_size > 0x5400u ||
        header_offset > runtime_size ||
        runtime_size-header_offset < RTS3_HEADER_SIZE ||
        rom_size == 0u || rom_size > 0x2000000u || (rom_size&3u) != 0u ||
        installed_address < 0x08000000u || (installed_address&3u) != 0u ||
        installed_address > 0x0A000000u-runtime_size)
        return 0;
    h=runtime+header_offset;
    {
        static const uint8_t magic[8]={'E','Z','R','T','S','3','K','0'};
        size_t i;
        for (i=0;i<8u;i++) if (h[i]!=magic[i]) return 0;
    }
    store32(h+0x14,game_code);
    store32(h+0x18,rom_size);
    store32(h+0x1C,rom_header_crc);
    store32(h+0x20,0u);
    store32(h+0x24,installed_address);
    store32(h+0x30,UINT32_C(0x000F0001));
    store32(h+0x34,RTS3_KERNEL_ABI);
    store32(h+0x38,(uint32_t)runtime_size);
    store32(h+0x3C,0u);
    crc=Rts3Crc32(runtime,runtime_size);
    store32(h+0x20,crc);
    return 1;
}

/* Explicit 16-bit writes for VRAM: byte writes are not supported there. */
static void put32_wide(uint8_t *p, uint32_t value)
{
    volatile uint16_t *out = (volatile uint16_t *)(void *)p;
    out[0] = (uint16_t)value;
    out[1] = (uint16_t)(value >> 16);
}
int Rts3FinalizeRuntime(uint8_t *runtime, size_t runtime_size,
    size_t header_offset, uint32_t game_code, uint32_t rom_size,
    uint32_t rom_header_crc, uint32_t installed_address)
{
    return finalize_runtime(runtime, runtime_size, header_offset, game_code,
        rom_size, rom_header_crc, installed_address, put32);
}
int Rts3FinalizeRuntime16(uint8_t *runtime, size_t runtime_size,
    size_t header_offset, uint32_t game_code, uint32_t rom_size,
    uint32_t rom_header_crc, uint32_t installed_address)
{
    if ((((uintptr_t)runtime | header_offset) & 1u) != 0u)
        return 0;
    return finalize_runtime(runtime, runtime_size, header_offset, game_code,
        rom_size, rom_header_crc, installed_address, put32_wide);
}
