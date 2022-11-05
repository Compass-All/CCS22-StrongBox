/*
 * Copyright (c) 2016-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Contains generic routines to fix up the device tree blob passed on to
 * payloads like BL32 and BL33 (and further down the boot chain).
 * This allows to easily add PSCI nodes, when the original DT does not have
 * it or advertises another method.
 * Also it supports to add reserved memory nodes to describe memory that
 * is used by the secure world, so that non-secure software avoids using
 * that.
 */

#include <string.h>

#include <libfdt.h>

#include <common/debug.h>
#include <drivers/console.h>
#include <lib/psci/psci.h>

#include <common/fdt_fixup.h>

static int append_psci_compatible(void *fdt, int offs, const char *str)
{
	return fdt_appendprop(fdt, offs, "compatible", str, strlen(str) + 1);
}

int dt_add_psci_node(void *fdt)
{
	int offs;

	if (fdt_path_offset(fdt, "/psci") >= 0) {
		WARN("PSCI Device Tree node already exists!\n");
		return 0;
	}

	offs = fdt_path_offset(fdt, "/");
	if (offs < 0)
		return -1;
	offs = fdt_add_subnode(fdt, offs, "psci");
	if (offs < 0)
		return -1;
	if (append_psci_compatible(fdt, offs, "arm,psci-1.0"))
		return -1;
	if (append_psci_compatible(fdt, offs, "arm,psci-0.2"))
		return -1;
	if (append_psci_compatible(fdt, offs, "arm,psci"))
		return -1;
	if (fdt_setprop_string(fdt, offs, "method", "smc"))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "cpu_suspend", PSCI_CPU_SUSPEND_AARCH64))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "cpu_off", PSCI_CPU_OFF))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "cpu_on", PSCI_CPU_ON_AARCH64))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "sys_poweroff", PSCI_SYSTEM_OFF))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "sys_reset", PSCI_SYSTEM_RESET))
		return -1;
	return 0;
}

/*
 * Find the first subnode that has a "device_type" property with the value
 * "cpu" and which's enable-method is not "psci" (yet).
 * Returns 0 if no such subnode is found, so all have already been patched
 * or none have to be patched in the first place.
 * Returns 1 if *one* such subnode has been found and successfully changed
 * to "psci".
 * Returns -1 on error.
 *
 * Call in a loop until it returns 0. Recalculate the node offset after
 * it has returned 1.
 */
static int dt_update_one_cpu_node(void *fdt, int offset)
{
	int offs;

	/* Iterate over all subnodes to find those with device_type = "cpu". */
	for (offs = fdt_first_subnode(fdt, offset); offs >= 0;
	     offs = fdt_next_subnode(fdt, offs)) {
		const char *prop;
		int len;

		prop = fdt_getprop(fdt, offs, "device_type", &len);
		if (!prop)
			continue;
		if (memcmp(prop, "cpu", 4) != 0 || len != 4)
			continue;

		/* Ignore any nodes which already use "psci". */
		prop = fdt_getprop(fdt, offs, "enable-method", &len);
		if (prop && memcmp(prop, "psci", 5) == 0 && len == 5)
			continue;

		if (fdt_setprop_string(fdt, offs, "enable-method", "psci"))
			return -1;
		/*
		 * Subnode found and patched.
		 * Restart to accommodate potentially changed offsets.
		 */
		return 1;
	}

	if (offs == -FDT_ERR_NOTFOUND)
		return 0;

	return offs;
}

int dt_add_psci_cpu_enable_methods(void *fdt)
{
	int offs, ret;

	do {
		offs = fdt_path_offset(fdt, "/cpus");
		if (offs < 0)
			return offs;

		ret = dt_update_one_cpu_node(fdt, offs);
	} while (ret > 0);

	return ret;
}

#define HIGH_BITS(x) ((sizeof(x) > 4) ? ((x) >> 32) : (typeof(x))0)

int fdt_add_reserved_memory(void *dtb, const char *node_name,
			    uintptr_t base, size_t size)
{
	int offs = fdt_path_offset(dtb, "/reserved-memory");
	uint32_t addresses[3];

	if (offs < 0) {			/* create if not existing yet */
		offs = fdt_add_subnode(dtb, 0, "reserved-memory");
		if (offs < 0)
			return offs;
		fdt_setprop_u32(dtb, offs, "#address-cells", 2);
		fdt_setprop_u32(dtb, offs, "#size-cells", 1);
		fdt_setprop(dtb, offs, "ranges", NULL, 0);
	}

	addresses[0] = cpu_to_fdt32(HIGH_BITS(base));
	addresses[1] = cpu_to_fdt32(base & 0xffffffff);
	addresses[2] = cpu_to_fdt32(size & 0xffffffff);
	offs = fdt_add_subnode(dtb, offs, node_name);
	fdt_setprop(dtb, offs, "no-map", NULL, 0);
	fdt_setprop(dtb, offs, "reg", addresses, 12);

	return 0;
}
