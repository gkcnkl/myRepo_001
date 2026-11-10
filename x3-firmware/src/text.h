#pragma once
#include <stddef.h>

// Converts UTF-8 to the single-byte encoding of our generated fonts:
// printable ASCII as-is, Turkish letters and · ° – to 0x80-0x8E
// (tools/fontgen), any other character to '?'. Always NUL-terminates `out`.
void utf8ToFont(const char* in, char* out, size_t outSize);

// Uppercases a font-encoded string in place. Turkish rules by default
// (i -> İ, ı -> I, ç -> Ç, ğ -> Ğ, ö -> Ö, ş -> Ş, ü -> Ü); turkish=false maps
// i -> I for other languages.
void upperFont(char* enc, bool turkish = true);
