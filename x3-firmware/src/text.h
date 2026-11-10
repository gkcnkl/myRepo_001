#pragma once
#include <stddef.h>

// Converts UTF-8 to the single-byte encoding of our generated fonts:
// printable ASCII as-is, Turkish letters to 0x80-0x8B (tools/fontgen), any
// other character to '?'. Always NUL-terminates `out`.
void utf8ToFont(const char* in, char* out, size_t outSize);
