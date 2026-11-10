// Tracks heap use by code outside the arenas (expat): link with
// -Wl,--wrap=malloc,--wrap=realloc,--wrap=free,--wrap=calloc
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
void* __real_malloc(size_t);
void* __real_realloc(void*, size_t);
void* __real_calloc(size_t, size_t);
void __real_free(void*);
static size_t cur, peak;
static int on;
void heaptrack_start(void) { on = 1; cur = peak = 0; }
size_t heaptrack_peak(void) { return peak; }
static void add(size_t n) { cur += n; if (cur > peak) peak = cur; }
void* __wrap_malloc(size_t n) { void* p = __real_malloc(n); if (on && p) add(malloc_usable_size(p)); return p; }
void* __wrap_calloc(size_t a, size_t b) { void* p = __real_calloc(a, b); if (on && p) add(malloc_usable_size(p)); return p; }
void* __wrap_realloc(void* q, size_t n) {
  size_t old = q ? malloc_usable_size(q) : 0;
  void* p = __real_realloc(q, n);
  if (on && p) { cur -= old < cur ? old : cur; add(malloc_usable_size(p)); }
  return p;
}
void __wrap_free(void* p) { if (on && p) { size_t n = malloc_usable_size(p); cur -= n < cur ? n : cur; } __real_free(p); }
