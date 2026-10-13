/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * prefix_registry — seed a Wine prefix's registry hives from the bundle.
 *
 * The app never runs wineboot, so the builtins' registrations (COM classes
 * above all: CoCreateInstance(CLSID_MMDeviceEnumerator) is how every title
 * reaches the audio driver) are generated at build time by
 * app/tools/prefix-registry.py into Runtime/registry/{system,user}.reg,
 * in wineserver's own file format. Before the wineserver starts, each seed
 * section whose key the prefix's hive does not have yet is appended to it.
 * A key it has is left alone, except a profile key, whose seed section
 * follows a ";; playport:top-up" line: it gets the seed's values it lacks.
 * A value the prefix already holds is never changed, so a user or title
 * change survives, and an app update that ships more classes adds only those.
 */

#ifndef PREFIX_REGISTRY_H
#define PREFIX_REGISTRY_H

#include <stddef.h>

/* Append to hive_path (created with the seed's header if missing) every
 * section of seed_path whose key it lacks, and a section holding the missing
 * values of each marked key it has. Returns the number of sections appended
 * (0 = already complete) or -1; err gets a one-line summary either way. */
int prefix_registry_seed(const char *seed_path, const char *hive_path, char *err, size_t errlen);

/* The same for values the host computes at every session start (wine_host.c
 * seed_machine_values, the values wineboot writes from the machine): every
 * key's section counts as marked, and a value the hive holds with other data
 * is appended again with the new data, which wineserver keeps as it loads
 * the later line. Values equal to the hive's are not written again. */
int prefix_registry_set(const char *values_path, const char *hive_path, char *err, size_t errlen);

/* The values wineboot's create_hardware_registry_keys writes from the machine
 * at every start, for cpus processors as an x86-64 guest sees them: the
 * Session Manager environment's NUMBER_OF_PROCESSORS and PROCESSOR_*, and
 * HARDWARE\DESCRIPTION\System with a CentralProcessor and a
 * FloatingPointProcessor key a processor. A system.reg text for
 * prefix_registry_set; returns its length, or -1 if len is too small. */
int prefix_registry_machine_values(unsigned cpus, char *buf, size_t len);

#endif
