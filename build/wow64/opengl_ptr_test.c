/* SPDX-License-Identifier: GPL-3.0-or-later */
/* opengl32's WoW64 pointer conversions (wine-unix 0029's unix_private.h), with
 * a mock TEB/TEB32 pair laid out as madeira-unix 0052 lays it out and a mock
 * context whose buffer bindings the test sets. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINE_IOS 1
typedef unsigned long ULONG_PTR;
typedef uint32_t ULONG;
typedef uint32_t PTR32;
typedef int BOOL;
typedef unsigned int GLenum;
typedef int GLint;
typedef struct { ULONG ExceptionList, StackBase, StackLimit, SubSystemTib, FiberData, ArbitraryUserPointer, Self; } NT_TIB32;
typedef struct { NT_TIB32 Tib; long WowTebOffset; } TEB32;
typedef struct { long WowTebOffset; const void *glTable; } TEB;
struct opengl_funcs { void (*p_glGetIntegerv)( GLenum pname, GLint *data ); };
#define PtrToUlong(p) ((ULONG)(ULONG_PTR)(p))

#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895

static TEB *current_teb;
static TEB *NtCurrentTeb(void) { return current_teb; }

static int rejected;
static PTR32 wow64_ptr32_rejected( const void *ptr )
{
    (void)ptr;
    rejected++;
    return 0;
}

static GLint array_buffer, element_buffer;
static int binding_queries;
static void mock_glGetIntegerv( GLenum pname, GLint *data )
{
    binding_queries++;
    if (pname == GL_ARRAY_BUFFER_BINDING) *data = array_buffer;
    else if (pname == GL_ELEMENT_ARRAY_BUFFER_BINDING) *data = element_buffer;
    else abort();
}

#include "opengl_api.h"

int main(void)
{
    /* a TEB and its TEB32 0x2000 above, the TEB32 at guest 0x7fe02000; the
     * pages stand in for the window, and base is what the pair implies */
    static _Alignas(16) char pages[0x4000];
    static const struct opengl_funcs funcs = { mock_glGetIntegerv };
    const ULONG guest_teb32 = 0x7fe02000;
    TEB *teb = (TEB *)pages;
    TEB32 *teb32 = (TEB32 *)(pages + 0x2000);
    ULONG_PTR base = (ULONG_PTR)teb32 - guest_teb32;
    static const ULONG edges[] = {1, 0x10000, 0x400000, 0x7bf40000, 0x7fffffff, 0x80000000, 0xffffffff};
    PTR32 guest_array[3] = {0x1000, 0, 0x2000};
    ULONG_PTR *copy;

    teb->WowTebOffset = 0x2000;
    teb->glTable = &funcs;
    teb32->Tib.Self = guest_teb32;
    teb32->WowTebOffset = -0x2000;
    current_teb = teb;
    assert( wow64_window_base() == base );
    /* the thunks' TEB parameter is the caller's TEB32 guest address */
    assert( get_teb64( guest_teb32 ) == teb );
    assert( wow64_ptr( 0 ) == NULL );
    assert( wow64_ptr32( NULL ) == 0 );
    for (unsigned int i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
    {
        void *host = wow64_ptr( edges[i] );
        assert( (ULONG_PTR)host == base + edges[i] );
        assert( wow64_ptr_fits( host ) );
        assert( wow64_ptr32( host ) == edges[i] );
        assert( wow64_ptr32_or_offset( host ) == edges[i] );
    }
    /* outside the window: NULL and a report, never a truncated pointer */
    assert( !wow64_ptr_fits( (void *)(base - 1) ) );
    assert( wow64_ptr32( (void *)(base - 1) ) == 0 && rejected == 1 );
    assert( wow64_ptr32( (void *)(base + UINT64_C(0x100000000)) ) == 0 && rejected == 2 );
    /* glGetPointerv: an offset into a bound buffer comes back as it is */
    assert( wow64_ptr32_or_offset( (void *)0x40 ) == 0x40 );
    assert( wow64_ptr32_or_offset( NULL ) == 0 && rejected == 2 );

    /* a pointer-or-offset parameter: a client pointer with no buffer bound,
     * an offset with one; NULL needs no query */
    binding_queries = 0;
    assert( (ULONG_PTR)wow64_ptr_or_offset( teb, GL_ARRAY_BUFFER_BINDING, 0x400000 ) == base + 0x400000 );
    array_buffer = 3;
    assert( (ULONG_PTR)wow64_ptr_or_offset( teb, GL_ARRAY_BUFFER_BINDING, 0x30 ) == 0x30 );
    assert( (ULONG_PTR)wow64_ptr_or_offset( teb, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0x30 ) == base + 0x30 );
    assert( wow64_ptr_or_offset( teb, GL_ELEMENT_ARRAY_BUFFER_BINDING, 0 ) == NULL );
    assert( binding_queries == 3 );

    /* arrays: pointers, offsets with a bound buffer, GLintptr values */
    memcpy( pages + 0x100, guest_array, sizeof(guest_array) );
    copy = copy_wow64_ptr32s( guest_teb32 - 0x2000 + 0x100, 3, wow64_window_base() );
    assert( copy[0] == base + 0x1000 && copy[1] == 0 && copy[2] == base + 0x2000 );
    free( copy );
    element_buffer = 5;
    copy = copy_wow64_ptr32s( guest_teb32 - 0x2000 + 0x100, 3, wow64_offset_base( teb, GL_ELEMENT_ARRAY_BUFFER_BINDING ) );
    assert( copy[0] == 0x1000 && copy[1] == 0 && copy[2] == 0x2000 );
    free( copy );
    copy = copy_wow64_ptr32s( guest_teb32 - 0x2000 + 0x100, 3, 0 );
    assert( copy[0] == 0x1000 && copy[2] == 0x2000 );
    free( copy );
    assert( copy_wow64_ptr32s( 0, 3, base ) == NULL );

    /* a thread without a TEB32 (the x86-64 session): identity, no query */
    teb->WowTebOffset = 0;
    binding_queries = 0;
    array_buffer = 0;
    assert( wow64_window_base() == 0 );
    assert( (ULONG_PTR)wow64_ptr( 0x400000 ) == 0x400000 );
    assert( wow64_ptr32( (void *)0x400000 ) == 0x400000 );
    assert( (ULONG_PTR)wow64_ptr_or_offset( teb, GL_ARRAY_BUFFER_BINDING, 0x400000 ) == 0x400000 );
    assert( binding_queries == 0 );
    assert( rejected == 2 );
    printf( "opengl32 guest window: base from the TEB pair, get_teb64, round trips, rejection, "
            "pointer-or-offset by binding, pointer arrays, no TEB32\n" );
    return EXIT_SUCCESS;
}
