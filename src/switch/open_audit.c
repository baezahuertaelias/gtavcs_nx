/* Every file open in the process passes through newlib's _open_r (fopen, freopen, open,
 * and libstdc++'s file streams on top of them). Linked with -Wl,--wrap=_open_r, this
 * counts the opens and records any whose path names a disc image (".iso", any case), so a
 * run can show on hardware that the folder disc replaced the ISO entirely (M04-13). */
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <reent.h>

int __real__open_r(struct _reent *reent, const char *path, int flags, int mode);

static atomic_uint total_opens, iso_opens;
static char first_iso[256];

static int names_iso(const char *path)
{
    const size_t n = path ? strlen(path) : 0;
    for (size_t i = 0; i + 4 <= n; ++i)
        if (path[i] == '.' && (path[i + 1] | 0x20) == 'i' && (path[i + 2] | 0x20) == 's' && (path[i + 3] | 0x20) == 'o' &&
            (i + 4 == n || path[i + 4] == ';' || path[i + 4] == '/'))
            return 1;
    return 0;
}

int __wrap__open_r(struct _reent *reent, const char *path, int flags, int mode)
{
    atomic_fetch_add(&total_opens, 1);
    if (names_iso(path) && atomic_fetch_add(&iso_opens, 1) == 0)
        snprintf(first_iso, sizeof(first_iso), "%s", path);
    return __real__open_r(reent, path, flags, mode);
}

unsigned gtavcs_open_count(void) { return atomic_load(&total_opens); }
unsigned gtavcs_iso_open_count(void) { return atomic_load(&iso_opens); }
const char *gtavcs_first_iso_open(void) { return first_iso; }
