# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the HKLM\\Software\\Wow6432Node mirror lookup shipped by wine-unix 0030 (PLA-177)."""
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
PATCH = REPO / "patches/wine-unix/0030-server-fall-back-from-HKLM-Software-Wow6432Node-to-H.patch"

# Just enough of server/registry.c's key tree for is_hklm_software and
# get_software_wow64_mirror, which the test takes verbatim from the patch.
HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint16_t WCHAR;
typedef unsigned int data_size_t;
struct unicode_str { const WCHAR *str; data_size_t len; };
struct object_name { struct object *parent; data_size_t len; WCHAR name[32]; };
struct object { const void *ops; struct object_name *name; };
struct key { struct object obj; struct key *subkeys[8]; int last_subkey; struct key *wow6432node; };
static const int key_ops, directory_ops;
static struct object directory = { &directory_ops, NULL };
static struct key *root_key;

static int memicmp_strW( const WCHAR *a, const WCHAR *b, data_size_t len )
{
    for (data_size_t i = 0; i < len / sizeof(WCHAR); i++)
    {
        WCHAR x = a[i] >= 'a' && a[i] <= 'z' ? a[i] - 32 : a[i];
        WCHAR y = b[i] >= 'a' && b[i] <= 'z' ? b[i] - 32 : b[i];
        if (x != y) return x - y;
    }
    return 0;
}

static struct key *get_parent( const struct key *key )
{
    struct object *parent = key->obj.name->parent;
    if (!parent || parent->ops != &key_ops) return NULL;
    return (struct key *)parent;
}

static struct key *find_subkey( const struct key *key, struct unicode_str name, int *index )
{
    for (int i = 0; i <= key->last_subkey; i++)
        if (key->subkeys[i]->obj.name->len == name.len &&
            !memicmp_strW( key->subkeys[i]->obj.name->name, name.str, name.len ))
        {
            *index = i;
            return key->subkeys[i];
        }
    return NULL;
}

static struct key *make_key( struct object *parent, const char *name )
{
    struct key *key = calloc( 1, sizeof(*key) );
    key->obj.ops = &key_ops;
    key->obj.name = calloc( 1, sizeof(*key->obj.name) );
    key->obj.name->parent = parent;
    for (size_t i = 0; name[i]; i++) key->obj.name->name[i] = name[i];
    key->obj.name->len = strlen( name ) * sizeof(WCHAR);
    key->last_subkey = -1;
    if (parent && parent->ops == &key_ops)
    {
        struct key *p = (struct key *)parent;
        p->subkeys[++p->last_subkey] = key;
        if (!strcmp( name, "Wow6432Node" )) p->wow6432node = key;
    }
    return key;
}
#define KEY(parent, name) make_key( &(parent)->obj, name )

PATCH_CODE

int main(void)
{
    root_key = make_key( &directory, "REGISTRY" );
    struct key *hklm = KEY( root_key, "Machine" ), *user = KEY( root_key, "User" );
    struct key *software = KEY( hklm, "Software" ), *microsoft = KEY( software, "Microsoft" );
    struct key *windows = KEY( microsoft, "Windows" );
    struct key *wow = KEY( software, "Wow6432Node" ), *wow_microsoft = KEY( wow, "microsoft" );
    struct key *wow_windows = KEY( wow_microsoft, "Windows" ), *wow_vendor = KEY( wow, "Vendor" );
    struct key *wow_game = KEY( wow_vendor, "Game" );
    struct key *user_software = KEY( user, "Software" ), *user_wow = KEY( user_software, "Wow6432Node" );
    struct key *classes = KEY( software, "Classes" ), *classes_wow = KEY( classes, "Wow6432Node" );

    assert( is_hklm_software( software ) );
    assert( !is_hklm_software( user_software ) && !is_hklm_software( microsoft ) );
    /* each key under HKLM\Software\Wow6432Node mirrors the key at its path under HKLM\Software */
    assert( get_software_wow64_mirror( wow ) == software );
    assert( get_software_wow64_mirror( wow_microsoft ) == microsoft );
    assert( get_software_wow64_mirror( wow_windows ) == windows );
    /* a 32-bit key whose path HKLM\Software lacks has no mirror */
    assert( !get_software_wow64_mirror( wow_vendor ) && !get_software_wow64_mirror( wow_game ) );
    /* the 64-bit keys, HKCU's Wow6432Node and Classes\Wow6432Node (KEY_WOWSHARE) are left alone */
    assert( !get_software_wow64_mirror( software ) && !get_software_wow64_mirror( windows ) );
    assert( !get_software_wow64_mirror( user_wow ) && !get_software_wow64_mirror( classes_wow ) );
    /* a deleted key has no parent */
    wow_windows->obj.name->parent = NULL;
    assert( !get_software_wow64_mirror( wow_windows ) );
    return 0;
}
'''


class SoftwareWow64Mirror(unittest.TestCase):
    def test_keys_under_hklm_software_wow6432node_mirror_hklm_software(self):
        added = "\n".join(line[1:] for line in PATCH.read_text().splitlines()
                          if line.startswith("+") and not line.startswith("+++"))
        code = "static int is_hklm_software(" + added.split("static int is_hklm_software(", 1)[1]
        code = code.split("\n#endif", 1)[0]
        self.assertIn("static struct key *get_software_wow64_mirror(", code)
        work = REPO / ".work"
        work.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="wow64-mirror-", dir=work) as tmp:
            root = Path(tmp)
            (root / "test.c").write_text(HARNESS.replace("PATCH_CODE", code))
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                            "-o", str(root / "test"), str(root / "test.c")],
                           check=True, capture_output=True)
            subprocess.run([str(root / "test")], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
