/* [EN] Minimal libc stand-ins so the host measurement link completes. Newlib
        is not available here; these are NOT what the board runs. They exist
        only so two configurations can be linked and compared, and they are
        identical in both, so the delta stays honest.
   [FA] جایگزین‌های کمینهٔ libc تا لینکِ اندازه‌گیری روی هاست کامل شود. newlib
        اینجا نیست؛ این‌ها آن چیزی نیستند که روی برد اجرا می‌شود. فقط برای آن‌اند
        که دو پیکربندی لینک و مقایسه شوند، و در هر دو یکسان‌اند تا اختلاف صادق بماند. */
#include <stddef.h>
void __libc_init_array(void) { }
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s;
    if (a < b) { while (n--) *a++ = *b++; } else { a += n; b += n; while (n--) *--a = *--b; } return d; }
int memcmp(const void *x, const void *y, size_t n) { const unsigned char *a = x, *b = y;
    while (n--) { if (*a != *b) return *a - *b; a++; b++; } return 0; }
size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
