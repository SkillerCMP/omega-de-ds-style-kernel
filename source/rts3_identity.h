#ifndef RTS3_IDENTITY_H
#define RTS3_IDENTITY_H
#include <stddef.h>
#include <stdint.h>
#define RTS3_KERNEL_ABI UINT32_C(0x138A0001)
#define RTS3_HEADER_SIZE 256u
#define RTS3_ROM_HEADER_SIZE 192u
uint32_t Rts3Crc32(const void *data, size_t size);
/* All bounds are checked before the first write. No unaligned structure access. */
int Rts3FinalizeRuntime(uint8_t *runtime, size_t runtime_size,
    size_t header_offset, uint32_t game_code, uint32_t rom_size,
    uint32_t rom_header_crc, uint32_t installed_address);
/* Halfword-aligned VRAM staging buffer; guaranteed volatile 16-bit stores. */
int Rts3FinalizeRuntime16(uint8_t *runtime, size_t runtime_size,
    size_t header_offset, uint32_t game_code, uint32_t rom_size,
    uint32_t rom_header_crc, uint32_t installed_address);
#endif
