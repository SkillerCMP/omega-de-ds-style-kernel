#ifndef DRAW_H
#define DRAW_H

#include <gba_base.h>

#define LIGHT

extern int current_y;
void Clear(u16 x, u16 y, u16 w, u16 h, u16 c, u8 isDrawDirect);
void ClearWithBG(u16* pbg,u16 x, u16 y, u16 w, u16 h, u8 isDrawDirect);
void DrawPic(u16 *GFX, u16 x, u16 y, u16 w, u16 h, u8 isTrans, u16 tcolor, u8 isDrawDirect);
void DrawHZText12(const char *str, u16 len, u16 x, u16 y, u16 c, u8 isDrawDirect);
void DrawHZText12ToBuffer(const char *str, u16 len, u16 x, u16 y, u16 c, u16 *buffer);
u16 DrawText12VisibleLength(const char *str);
u16 DrawText12ByteOffsetForGlyphs(const char *str, u16 glyphs);
void DrawText12CopyVisible(char *dst, u16 dst_size, const char *src, u16 glyphs);
void DEBUG_printf(const char *format, ...);
void ShowbootProgress(const char *str);
extern void wait_btn(void);

#endif /* DRAW_H */
