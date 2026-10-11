/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dotnet_fakedlls — the .NET Framework directories of Wine Mono's support
 * package (support/dotnetfakedlls.inf), decision 0085.
 *
 * A wineboot'ed prefix gets them from the package's installer: setupapi runs
 * the inf's [DefaultInstall] (the 32-bit side) and [DefaultInstall.ntamd64]
 * (the 64-bit side), whose WineFakeDlls= sections name a directory under
 * C:\windows (dirid 10) and a Wine builtin for each line, put there as a
 * fake DLL: C:\windows\Microsoft.NET\Framework\v4.0.30319\fusion.dll, which
 * mscoree's LoadLibraryShim loads, and the like. The app never runs wineboot,
 * so the session links each name into the runtime's DLL set of that side, as
 * system32 and syswow64 are linked. A link of an earlier bundle path is
 * replaced; any other file there (a real .NET install's) stays, as setupapi
 * keeps a file that is not a fake DLL.
 */

#ifndef DOTNET_FAKEDLLS_H
#define DOTNET_FAKEDLLS_H

#include <stddef.h>

/* Link the inf at inf_path into windows_dir (the prefix's C:\windows):
 * [DefaultInstall]'s names to dll_dir32/<name>, [DefaultInstall.ntamd64]'s
 * to dll_dir64/<name>. Returns the number of links made, or -1 when the inf
 * cannot be read or names neither section; err gets a one-line summary
 * either way (lines skipped: another dirid, an unsafe path, no such builtin,
 * a file that is not a link). */
int dotnet_fakedlls_link(const char *inf_path, const char *windows_dir,
                         const char *dll_dir32, const char *dll_dir64, char *err, size_t errlen);

#endif
