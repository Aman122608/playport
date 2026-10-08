/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Compile wine-pe 0032's wow64_NtUserThunkedMenuItemInfo with wine-pe 0018's
 * window helpers, from the patched user.c and wow64win_private.h. The mock of
 * win32u's NtUserThunkedMenuItemInfo reads and writes dwTypeData as win32u's
 * set_menu_item_info and get_menu_item_info do. An item's text converts through
 * the window only where win32u dereferences it; a get gives the guest its own
 * buffer back, or the bitmap handle or 0 that MIIM_TYPE leaves in its place. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t UINT;
typedef uint32_t UINT32;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef uint16_t USHORT;
typedef uint16_t WCHAR;
typedef int BOOL;
typedef int32_t NTSTATUS;
typedef uintptr_t ULONG_PTR;
typedef uint64_t ULONG64;
typedef void *HANDLE;
typedef void *HMENU;
typedef void *HBITMAP;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define ULongToPtr(value) ((void *)(uintptr_t)(ULONG)(value))
#define UlongToPtr(value) ULongToPtr(value)
#define PtrToUlong(value) ((ULONG)(uintptr_t)(value))
#define LongToHandle(value) ((HANDLE)(intptr_t)(LONG)(value))
#define UlongToHandle(value) ((HANDLE)(uintptr_t)(ULONG)(value))
#define HandleToUlong(value) ((ULONG)(uintptr_t)(value))

#define MIIM_STATE      0x0001
#define MIIM_ID         0x0002
#define MIIM_SUBMENU    0x0004
#define MIIM_CHECKMARKS 0x0008
#define MIIM_TYPE       0x0010
#define MIIM_DATA       0x0020
#define MIIM_STRING     0x0040
#define MIIM_BITMAP     0x0080
#define MIIM_FTYPE      0x0100
#define MFT_BITMAP      0x0004
#define MFT_SEPARATOR   0x0800
#define MF_BYPOSITION   0x0400

/* ntuser.h's NtUserThunkedMenuItemInfo codes */
enum
{
    NtUserSetMenuItemInfo,
    NtUserInsertMenuItem,
    NtUserCheckMenuRadioItem,
    NtUserGetMenuDefaultItem,
    NtUserGetMenuItemID,
    NtUserGetMenuItemInfoA,
    NtUserGetMenuItemInfoW,
    NtUserGetMenuState,
    NtUserGetSubMenu,
};

typedef struct
{
    UINT cbSize;
    UINT fMask;
    UINT fType;
    UINT fState;
    UINT wID;
    HMENU hSubMenu;
    HBITMAP hbmpChecked;
    HBITMAP hbmpUnchecked;
    ULONG_PTR dwItemData;
    WCHAR *dwTypeData;
    UINT cch;
    HBITMAP hbmpItem;
} MENUITEMINFOW;

typedef struct
{
    USHORT Length;
    USHORT MaximumLength;
    WCHAR *Buffer;
} UNICODE_STRING;

typedef struct
{
    USHORT Length;
    USHORT MaximumLength;
    ULONG Buffer;
} UNICODE_STRING32;

static ULONG64 wow64_window_base;
static int rejected;
static ULONG wow64_guest_rejected(const void *host)
{
    (void)host;
    rejected++;
    return 0;
}

/* win32u's side of one menu item */
enum item_kind { ITEM_TEXT, ITEM_BITMAP, ITEM_SEPARATOR };
static struct
{
    enum item_kind kind;
    WCHAR text[16];
    int calls;
    UINT method, pos, flags;
    const WCHAR *passed_text;
    ULONG_PTR item_data;
    UNICODE_STRING *str;
} item;

static const WCHAR play[] = {'P', 'l', 'a', 'y', 0};

static UINT wcslen16(const WCHAR *s)
{
    UINT n = 0;
    while (s[n]) n++;
    return n;
}

static UINT NtUserThunkedMenuItemInfo(HMENU handle, UINT pos, UINT flags, UINT method,
                                      MENUITEMINFOW *info, UNICODE_STRING *str)
{
    (void)handle;
    item.calls++;
    item.method = method;
    item.pos = pos;
    item.flags = flags;
    item.str = str;
    item.passed_text = info ? info->dwTypeData : NULL;
    switch (method)
    {
    case NtUserInsertMenuItem:
    case NtUserSetMenuItemInfo:
        /* set_menu_item_info: the text is read only under MIIM_STRING */
        assert(info && info->cbSize == sizeof(*info));
        if (info->fMask & MIIM_STRING)
        {
            if (info->dwTypeData) memcpy(item.text, info->dwTypeData, (wcslen16(info->dwTypeData) + 1) * sizeof(WCHAR));
            else item.text[0] = 0;
        }
        if (info->fMask & MIIM_DATA) item.item_data = info->dwItemData;
        return TRUE;
    case NtUserGetMenuItemInfoW:
        /* get_menu_item_info */
        assert(info && info->cbSize == sizeof(*info));
        if (info->fMask & MIIM_TYPE)
        {
            if (item.kind == ITEM_BITMAP)
            {
                info->dwTypeData = (WCHAR *)(uintptr_t)0x2a;
                info->cch = 0;
            }
            else if (item.kind == ITEM_SEPARATOR)
            {
                info->dwTypeData = NULL;
                info->cch = 0;
            }
        }
        if (info->fMask & (MIIM_TYPE | MIIM_STRING))
        {
            if (item.kind != ITEM_TEXT)
            {
                if (info->dwTypeData && info->cch) info->dwTypeData[0] = 0;
                info->cch = 0;
            }
            else
            {
                UINT len = wcslen16(item.text), n;
                if (info->dwTypeData && info->cch)
                {
                    for (n = 0; n + 1 < info->cch && item.text[n]; n++) info->dwTypeData[n] = item.text[n];
                    info->dwTypeData[n] = 0;
                    info->cch = info->cch <= len + 1 ? info->cch - 1 : len;
                }
                else info->cch = len;
            }
        }
        if (info->fMask & MIIM_DATA) info->dwItemData = item.item_data;
        return TRUE;
    case NtUserCheckMenuRadioItem:
        assert(info && info->cch == 7 && info->fMask == 3);
        return TRUE;
    default:
        assert(!info);
        return 5;
    }
}

#include "wow64_window.h"
#include "menu_item_api.h"

/* The guest's address space: a MENUITEMINFOW32 and text buffers. */
static _Alignas(16) unsigned char arena[256];
#define GUEST 0x00100000u
#define INFO32 (GUEST + 0x00)
#define TEXT32 (GUEST + 0x40)
#define BUFFER32 (GUEST + 0x80)

static void *host(ULONG guest) { return arena + (guest - GUEST); }
static MENUITEMINFOW32 *info32(void) { return host(INFO32); }

static UINT call(UINT method, UINT pos, UINT flags, ULONG info, ULONG str)
{
    UINT args[] = {0x10034, pos, flags, method, info, str};
    return (UINT)wow64_NtUserThunkedMenuItemInfo(args);
}

static void reset(UINT fmask, ULONG type_data)
{
    memset(arena, 0, sizeof(arena));
    memset(&item, 0, sizeof(item));
    info32()->cbSize = sizeof(MENUITEMINFOW32);
    info32()->fMask = fmask;
    info32()->dwTypeData = type_data;
}

int main(void)
{
    static const WCHAR stale[] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 0};
    WCHAR *buffer;

    assert(sizeof(MENUITEMINFOW32) == 48);
    wow64_window_base = (uintptr_t)arena - GUEST;
    assert(wow64_window_base > UINT32_MAX);

    /* Super Meat Boy's AppendMenuW: InsertMenuItem by position -1 with text. */
    reset(MIIM_STATE | MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP | MIIM_CHECKMARKS, TEXT32);
    memcpy(host(TEXT32), play, sizeof(play));
    assert(call(NtUserInsertMenuItem, 0xffffffff, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == host(TEXT32) && item.pos == 0xffffffff && item.flags == MF_BYPOSITION);
    assert(memcmp(item.text, play, sizeof(play)) == 0 && item.str == NULL);
    /* A separator: MIIM_STRING with no text. */
    reset(MIIM_FTYPE | MIIM_STRING, 0);
    assert(call(NtUserSetMenuItemInfo, 1, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == NULL && item.text[0] == 0);
    /* MIIM_TYPE with MFT_BITMAP after user32's normalizing: dwTypeData holds the
     * bitmap handle, win32u never reads it and it does not convert. */
    reset(MIIM_TYPE | MIIM_FTYPE | MIIM_BITMAP, 0x2a);
    info32()->fType = MFT_BITMAP;
    info32()->hbmpItem = 0x2a;
    assert(call(NtUserInsertMenuItem, 2, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == (WCHAR *)(uintptr_t)0x2a);
    /* Item data is the guest's, kept and given back as is. */
    reset(MIIM_DATA, 0);
    info32()->dwItemData = 0x00401234;
    assert(call(NtUserSetMenuItemInfo, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.item_data == 0x00401234);
    assert(rejected == 0);

    /* GetMenuStringW: win32u writes the text into the guest's buffer. */
    reset(MIIM_STRING, BUFFER32);
    memcpy(item.text, play, sizeof(play));
    info32()->cch = 16;
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == host(BUFFER32));
    assert(info32()->dwTypeData == BUFFER32 && info32()->cch == 4);
    assert(memcmp(host(BUFFER32), play, sizeof(play)) == 0);
    /* MIIM_TYPE on a text item: the same buffer comes back, truncated to cch. */
    reset(MIIM_TYPE, BUFFER32);
    memcpy(item.text, play, sizeof(play));
    info32()->cch = 3;
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(info32()->dwTypeData == BUFFER32 && info32()->cch == 2);
    buffer = host(BUFFER32);
    assert(buffer[0] == 'P' && buffer[1] == 'l' && buffer[2] == 0);
    /* MIIM_TYPE on a bitmap item: the handle, not a guest pointer, comes back. */
    reset(MIIM_TYPE, BUFFER32);
    memcpy(host(BUFFER32), stale, sizeof(stale));
    item.kind = ITEM_BITMAP;
    info32()->cch = 8;
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(info32()->dwTypeData == 0x2a && info32()->cch == 0);
    assert(memcmp(host(BUFFER32), stale, sizeof(stale)) == 0);
    /* MIIM_TYPE on a separator: 0 comes back. */
    reset(MIIM_TYPE, BUFFER32);
    item.kind = ITEM_SEPARATOR;
    info32()->cch = 8;
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(info32()->dwTypeData == 0 && info32()->cch == 0);
    /* The length only: no buffer. */
    reset(MIIM_STRING, 0);
    memcpy(item.text, play, sizeof(play));
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == NULL && info32()->dwTypeData == 0 && info32()->cch == 4);
    /* No text asked for: whatever the guest left there is not converted and
     * stays as it was. */
    reset(MIIM_STATE | MIIM_DATA, 0xdeadbeef);
    assert(call(NtUserGetMenuItemInfoW, 0, MF_BYPOSITION, INFO32, 0) == TRUE);
    assert(item.passed_text == (WCHAR *)(uintptr_t)0xdeadbeef && info32()->dwTypeData == 0xdeadbeef);
    assert(rejected == 0);

    /* CheckMenuRadioItem passes last and check in cch and fMask. */
    reset(3, 0);
    info32()->cch = 7;
    assert(call(NtUserCheckMenuRadioItem, 1, MF_BYPOSITION, INFO32, 0) == TRUE);
    /* Methods without an info block. */
    reset(0, 0);
    assert(call(NtUserGetMenuItemID, 0, MF_BYPOSITION, 0, 0) == 5);
    assert(call(NtUserGetMenuState, 0, MF_BYPOSITION, 0, 0) == 5);

    /* No window: a guest address is the host address. */
    wow64_window_base = 0;
    assert(menu_item_text_32to64(0x00412340, TRUE) == (WCHAR *)(uintptr_t)0x00412340);
    assert(menu_item_text_64to32((WCHAR *)(uintptr_t)0x00412340, (WCHAR *)(uintptr_t)0x00412340, 0x00412340) == 0x00412340);
    assert(rejected == 0);
    puts("menu item thunk: insert text, separator, bitmap, item data, get text, truncation, bitmap, separator, length, no text, radio, no info, no window");
    return 0;
}
