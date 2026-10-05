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

/* [EN] v1.58 audit: task_control.c divides a uint64_t, so the compiler calls
        the EABI 64-bit divide helper that lives in libgcc. libgcc is absent
        here too, and without a stand-in the measurement link fails and the
        whole "does it fit?" question cannot be answered at all. This naive
        shift-subtract version is only ever linked by the measuring tool.
   [FA] ممیزی v1.58: task_control.c یک تقسیم ۶۴ بیتی دارد و کامپایلر کمک‌تابع
        libgcc را صدا می‌زند که اینجا نیست؛ بدون جایگزین، لینک اندازه‌گیری
        شکست می‌خورد و اصلاً نمی‌شود گفت برنامه جا می‌شود یا نه. */
unsigned long long __aeabi_uldivmod(unsigned long long n, unsigned long long d);
unsigned long long __aeabi_uldivmod(unsigned long long n, unsigned long long d)
{
    unsigned long long q = 0u, r = 0u;
    int i;
    if (d == 0u) { return 0u; }
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= (1ull << i); }
    }
    return q;
}
