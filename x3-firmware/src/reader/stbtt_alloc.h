#pragma once
// Force-included into every translation unit (build flag -include) so the
// stb_truetype implementation inside FreeInkFont's TtfFont.cpp uses these
// instead of malloc/free/assert.
//
// Why: rasterizing one glyph, stb asks for a 2000-entry active-edge chunk;
// on the 32-bit ESP32-C3 that is one ~56 KB block (25 KB on a 64-bit PC),
// which the X3's fragmented heap never has, and STBTT_assert(z != NULL) then
// aborts. src/reader/book_reader.cpp serves these from the idle parse arena
// while a page is drawn (stb frees everything after each glyph), and falls
// back to malloc otherwise. A failed allocation leaves a glyph blank instead
// of aborting.
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
void* x3_stbtt_malloc(size_t size);
void x3_stbtt_free(void* ptr);
#ifdef __cplusplus
}
#endif

#define STBTT_malloc(x, u) ((void)(u), x3_stbtt_malloc(x))
#define STBTT_free(x, u) ((void)(u), x3_stbtt_free(x))
#define STBTT_assert(x) ((void)0)
