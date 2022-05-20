// SPDX-License-Identifier: GPL-2.0-only

#include <debug.h>
#include <list.h>
#include <kernel/mutex.h>

#include <lib/bio.h>
#include <lib/fs.h>

#include "boot.h"

static void dump_devices()
{
	struct bdev_struct *bdevs = bio_get_bdevs();
	bdev_t *entry;

	dprintf(INFO, "block devices:\n");
	mutex_acquire(&bdevs->lock);
	dprintf(INFO, " | dev    | label      | size      | S |\n");
	list_for_every_entry(&bdevs->list, entry, bdev_t, node) {
		dprintf(INFO, " | %-6s | %-10s | %5lld %s | %s |\n",
			entry->name,
			entry->label,
			entry->size / (entry->size > 1024 * 1024 ? 1024*1024 : 1024),
			(entry->size > 1024 * 1024 ? "MiB" : "KiB"),
			(entry->is_subdev ? "X" : " ")
			);
	}
	mutex_release(&bdevs->lock);
}

int bootapp_init()
{
	dprintf(INFO, "Reached bootapp init!\n");
	dump_devices();
	return 0;
}
