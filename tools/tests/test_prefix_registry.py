# SPDX-License-Identifier: GPL-3.0-or-later
"""app/tools/prefix-registry.py: the NEUTRALISE keys (decision 0071) in the
committed user.reg seed, marked for top-up, with the values that keep winedbg's
crash dialog, winemenubuilder and control.exe's add-on download off the phone;
the Visual C++ runtime key (VC_RUNTIME), wintrust's SIP providers
(register_wintrust_sips) and the Windows Imaging Component
registration (register_wic, decision 0072) in the committed system.reg seed,
and its reading of windowscodecs' regsvr.c tables."""
import importlib.util
from pathlib import Path
import unittest

REPO = Path(__file__).resolve().parents[2]
MARKER = ";; playport:top-up"


def sections(text):
    """[key] -> (marked, value lines) of a wineserver registry file."""
    out, key, marked, prev = {}, None, False, ""
    for line in text.splitlines():
        if line.startswith("["):
            key, marked = line[1:line.rindex("]")], prev == MARKER
            out[key] = (marked, [])
        elif key and line and not line.startswith(("#", ";")):
            out[key][1].append(line)
        prev = line
    return out


class Neutralise(unittest.TestCase):
    def test_user_seed_turns_off_the_crash_dialog_menubuilder_and_addon_installer(self):
        user = sections((REPO / "app/registry/user.reg").read_text())
        marked, values = user["Software\\\\Wine\\\\WineDbg"]
        self.assertTrue(marked)
        self.assertIn('"ShowCrashDialog"=dword:00000000', values)
        marked, values = user["Software\\\\Wine\\\\DllOverrides"]
        self.assertTrue(marked)
        self.assertIn('"control.exe"=""', values)
        self.assertIn('"winemenubuilder.exe"=""', values)


class VcRuntime(unittest.TestCase):
    def test_system_seed_reports_the_vc_runtime_installed_as_wine_inf_does(self):
        system = sections((REPO / "app/registry/system.reg").read_text())
        marked, values = system["Software\\\\Microsoft\\\\VisualStudio\\\\14.0\\\\VC\\\\Runtimes\\\\x64"]
        self.assertTrue(marked)
        self.assertIn('"Installed"=dword:00000001', values)
        self.assertIn('"Major"=dword:0000000e', values)
        self.assertIn('"Minor"=dword:0000002a', values)
        self.assertIn('"Rbld"=dword:00000000', values)


class WintrustSips(unittest.TestCase):
    def test_pe_image_sip_names_wintrusts_functions_so_cryptqueryobject_finds_a_signature(self):
        system = sections((REPO / "app/registry/system.reg").read_text())
        key = ("Software\\\\Microsoft\\\\Cryptography\\\\OID\\\\EncodingType 0\\\\"
               "CryptSIPDllGetSignedDataMsg\\\\{C689AAB8-8E78-11D0-8C47-00C04FC295EE}")
        _marked, values = system[key]
        self.assertEqual(values, ['"Dll"="WINTRUST.DLL"', '"FuncName"="CryptSIPGetSignedDataMsg"'])


CLSID = "Software\\\\Classes\\\\CLSID\\\\"
PNG_DECODER = "{389EA17B-5078-4CDE-B6EF-25C15175C751}"
CATID_WIC_BITMAP_DECODERS = "{7ED96837-96F0-4812-B211-F13C24117ED3}"


class WicSeed(unittest.TestCase):
    def test_png_decoder_is_an_instance_of_the_decoder_category_in_the_64_bit_classes(self):
        system = sections((REPO / "app/registry/system.reg").read_text())
        marked, values = system[f"{CLSID}{CATID_WIC_BITMAP_DECODERS}\\\\Instance\\\\{PNG_DECODER}"]
        self.assertTrue(marked)
        self.assertEqual(values, [f'"CLSID"="{PNG_DECODER}"'])
        marked, values = system[f"{CLSID}{PNG_DECODER}\\\\Patterns\\\\0"]
        self.assertTrue(marked)
        self.assertIn('"Pattern"=hex:89,50,4e,47,0d,0a,1a,0a', values)
        self.assertIn('"Mask"=hex:ff,ff,ff,ff,ff,ff,ff,ff', values)
        self.assertIn('"Length"=dword:00000008', values)

    def test_existing_decoder_class_key_is_marked_so_a_prefix_gets_its_new_values(self):
        marked, values = sections((REPO / "app/registry/system.reg").read_text())[f"{CLSID}{PNG_DECODER}"]
        self.assertTrue(marked)
        self.assertIn('"ContainerFormat"="{1B7CFAF4-713F-473C-BBCD-6137425FAEAF}"', values)

    def test_no_32_bit_class_keys_hide_the_shared_classes_from_i386_callers(self):
        self.assertNotIn("Wow6432Node", (REPO / "app/registry/system.reg").read_text())


REGSVR = r"""
struct decoder_pattern
{
    DWORD length;    /* 0 for end of list */
    DWORD position;
    const BYTE *pattern;
    const BYTE *mask;
    DWORD endofstream;
};
struct regsvr_thing
{
    CLSID const *clsid;         /* NULL for end of list */
    LPCSTR friendlyname;
    GUID const * const *formats;
    const struct decoder_pattern *patterns;
};
static const BYTE mask_all[] = {0xff,0xff,0xff,0xff};
static const BYTE ico_magic[] = {00,00,01,00};
static const BYTE gif_magic[3] = "GIF";
static const BYTE dds_magic[] = "DDS";
static GUID const * const formats[] = {
    &GUID_A,
    NULL
};
static struct decoder_pattern const patterns[] = {
    {4,0,ico_magic,mask_all,0},
    {3,2,gif_magic,mask_all,1},
    {0}
};
static struct regsvr_thing const thing_list[] = {
    {   &CLSID_Thing,
        "Thing \"One\"",
        formats,
        patterns
    },
    {   &CLSID_Other },
    { NULL }    /* list terminator */
};
"""
HEADER = """DEFINE_GUID(GUID_A, 0x6fddc324,0x4e03,0x4bfe,0xb1,0x85,0x3d,0x77,0x76,0x8d,0xc9,0x0f);
DEFINE_GUID(CLSID_Thing, 0x389ea17b,0x5078,0x4cde,0xb6,0xef,0x25,0xc1,0x51,0x75,0xc7,0x51);
DEFINE_GUID(CLSID_Other, 0x00000001,0x0002,0x0003,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b);
"""


@unittest.skipUnless(importlib.util.find_spec("pefile"), "prefix-registry.py imports pefile")
class WicSource(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("prefix_registry", REPO / "app/tools/prefix-registry.py")
        self.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.mod)

    def test_struct_tables_resolve_guids_byte_arrays_and_stop_at_the_terminator(self):
        src = self.mod.WicSource(REGSVR, [HEADER])
        rows = src.entries("thing_list", "regsvr_thing", "clsid")
        self.assertEqual([r["clsid"] for r in rows], [
            "{389EA17B-5078-4CDE-B6EF-25C15175C751}", "{00000001-0002-0003-0405-060708090A0B}"])
        self.assertEqual(rows[0]["friendlyname"], b'Thing "One"')
        self.assertEqual(src.guid_list(rows[0]["formats"]), ["{6FDDC324-4E03-4BFE-B185-3D77768DC90F}"])
        self.assertIsNone(rows[1]["patterns"])
        pats = src.entries(rows[0]["patterns"], "decoder_pattern", "length")
        self.assertEqual([(p["length"], p["position"], p["pattern"], p["endofstream"]) for p in pats],
                         [(4, 0, b"\x00\x00\x01\x00", 0), (3, 2, b"GIF", 1)])
        self.assertEqual(src.array("dds_magic"), b"DDS\x00")

    def test_a_pattern_longer_than_its_table_fails(self):
        with self.assertRaises(SystemExit):
            self.mod.fixed(b"GIF", 4, "test")

    def test_changed_register_code_stops_the_generation(self):
        regsvr = "".join(f"static HRESULT {fn}(void *list)\n{{\n}}\n" for fn in self.mod.WIC_FUNCTIONS[:-1])
        regsvr += "HRESULT WINAPI DllRegisterServer(void)\n{\n}\n"
        with self.assertRaisesRegex(SystemExit, "register_\\* code changed"):
            self.mod.register_wic(self.mod.Hive(), regsvr, [HEADER])


if __name__ == "__main__":
    unittest.main()
