/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Compile wine-pe 0037's wow64_NtGdiDdDDICreateDCFromMemory with wine-pe 0018's
 * window helpers, from the patched gdi.c and wow64win_private.h. The surface's
 * bits and colour table are guest pointers and convert through the window (a
 * NULL table stays NULL); the device DC is a handle, and the DC and bitmap
 * win32u returns go back to the guest's descriptor. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int32_t NTSTATUS;
typedef uint64_t ULONG64;
typedef void *HANDLE;
typedef void *HDC;
typedef void *HBITMAP;
typedef UINT D3DDDIFORMAT;
typedef struct { unsigned char rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
#define WINAPI
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define UlongToPtr(value) ((void *)(uintptr_t)(ULONG)(value))
#define ULongToPtr(value) UlongToPtr(value)
#define PtrToUlong(value) ((ULONG)(uintptr_t)(value))
#define UlongToHandle(value) ((HANDLE)(uintptr_t)(ULONG)(value))
#define HandleToUlong(value) ((ULONG)(uintptr_t)(value))

typedef struct
{
    void *pMemory;
    D3DDDIFORMAT Format;
    UINT Width;
    UINT Height;
    UINT Pitch;
    HDC hDeviceDc;
    RGBQUAD *pColorTable;
    HDC hDc;
    HBITMAP hBitmap;
} D3DKMT_CREATEDCFROMMEMORY;

static ULONG64 wow64_window_base;
static int rejected;
static ULONG wow64_guest_rejected(const void *host)
{
    (void)host;
    rejected++;
    return 0;
}

static D3DKMT_CREATEDCFROMMEMORY seen;
static int calls;

static NTSTATUS NtGdiDdDDICreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY *desc)
{
    calls++;
    seen = *desc;
    desc->hDc = (HDC)(uintptr_t)0x01010040;
    desc->hBitmap = (HBITMAP)(uintptr_t)0x0a050077;
    return 0;
}

#include "wow64_window.h"
#include "dc_from_memory_api.h"

/* The guest's 4 GB, as the window maps it: guest address n is window[n]. */
static unsigned char window[0x10000] __attribute__((aligned(16)));

int main(void)
{
    enum { DESC = 0x100, BITS = 0x1000, TABLE = 0x3000 };
    ULONG *desc32 = (ULONG *)(window + DESC);
    UINT args[1] = { DESC };

    wow64_window_base = (ULONG64)(uintptr_t)window;

    /* pMemory, Format, Width, Height, Pitch, hDeviceDc, pColorTable, hDc, hBitmap */
    ULONG init[9] = { BITS, 41, 64, 32, 64, 0x01010020, TABLE, 0, 0 };
    memcpy(desc32, init, sizeof(init));
    assert(wow64_NtGdiDdDDICreateDCFromMemory(args) == 0);
    assert(calls == 1);
    assert(seen.pMemory == window + BITS);
    assert(seen.pColorTable == (RGBQUAD *)(window + TABLE));
    assert(seen.Format == 41 && seen.Width == 64 && seen.Height == 32 && seen.Pitch == 64);
    assert(seen.hDeviceDc == (HDC)(uintptr_t)0x01010020);
    assert(desc32[7] == 0x01010040 && desc32[8] == 0x0a050077);

    /* No colour table: NULL, not the window's base. */
    desc32[6] = 0;
    assert(wow64_NtGdiDdDDICreateDCFromMemory(args) == 0);
    assert(seen.pColorTable == NULL);
    assert(seen.pMemory == window + BITS);

    /* No descriptor. */
    args[0] = 0;
    assert(wow64_NtGdiDdDDICreateDCFromMemory(args) == STATUS_INVALID_PARAMETER);
    assert(calls == 2 && rejected == 0);

    printf("DC from memory thunk: bits and colour table through the window, NULL table, handles back, no descriptor\n");
    return 0;
}
