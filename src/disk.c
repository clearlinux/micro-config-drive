/***
 Copyright © 2015 Intel Corporation

 Author: Julio Montes <julio.montes@intel.com>

 This file is part of micro-config-drive.

 micro-config-drive is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 micro-config-drive is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with micro-config-drive. If not, see <http://www.gnu.org/licenses/>.

 In addition, as a special exception, the copyright holders give
 permission to link the code of portions of this program with the
 OpenSSL library under certain conditions as described in each
 individual source file, and distribute linked combinations
 including the two.
 You must obey the GNU General Public License in all respects
 for all of the code used other than OpenSSL.  If you modify
 file(s) with this exception, you may extend this exception to your
 version of the file(s), but you are not obligated to do so.  If you
 do not wish to do so, delete this exception statement from your
 version.  If you delete this exception statement from all source
 files in the program, then also delete it here.
***/

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <sys/stat.h>

#include <glib.h>
#include <blkid.h>
#include <parted/parted.h>

#include "lib.h"
#include "async_task.h"

#define MOD "disk: "

static gboolean resize_fs;

static PedExceptionOption disk_exception_handler(PedException* ex) {
	const gchar* warning_message = "Not all of the space available";
	LOG("Warning: %s\n", ex->message);
	if (strncmp(ex->message, warning_message, strlen(warning_message)) == 0) {
		if (PED_EXCEPTION_WARNING == ex->type && ex->options & PED_EXCEPTION_FIX) {
			LOG("Handling warning, filesystem must be resized\n");
			resize_fs = true;
			return PED_EXCEPTION_FIX;
		}
	}
	LOG("Warning was not handled\n");
	return PED_EXCEPTION_UNHANDLED;
}

char *disk_by_path(const gchar* path) {
	char diskname[NAME_MAX];
    struct stat st = { 0 };
    dev_t disk;

    if (!path) {
		LOG(MOD "Path is empty\n");
        return NULL;
    }
    if (stat(path, &st) != 0) {
		LOG(MOD "Cannot stat '%s'\n", path);
        return NULL;
    }
    if (blkid_devno_to_wholedisk(st.st_dev, diskname, sizeof(diskname), &disk) != 0) {
		LOG(MOD "Cannot convert devno to wholedisk\n");
		return NULL;
    }

    return blkid_devno_to_devname(disk);
}

gboolean disk_fix(const gchar* disk_path) {
	char command[LINE_MAX] = { 0 };
	int last_partition_num;
	const gchar* partition_path;
	PedDevice* dev = NULL;
	PedDisk* disk = NULL;
	gboolean result = false;
	PedPartition* partition;
	PedSector start;
	PedSector end;
	PedGeometry geometry_start;
	PedGeometry* geometry_end;
	PedConstraint* constraint;

	resize_fs = false;

	/* to handle exceptions, i.e Fix PMBR */
	ped_exception_set_handler(disk_exception_handler);

	if (!disk_path) {
		LOG(MOD "Disk path is empty\n");
		return false;
	}

	dev = ped_device_get(disk_path);

	if (!dev) {
		LOG(MOD "Cannot get device '%s'\n", disk_path);
		return false;
	}

	/*
	* verify disk, disk_exception_handler will called
	* if the disk has problems and it needs to be fixed
	* and resized
	*/
	disk = ped_disk_new(dev);

	if (!disk) {
		LOG(MOD "Cannot create a new disk '%s'\n", disk_path);
		return false;
	}

	if (!resize_fs) {
		/* do not resize filesystem, it is ok */
		LOG(MOD "Nothing to do with '%s' disk\n", disk_path);
		return false;
	}

	LOG(MOD "Resizing filesystem disk '%s'\n", disk_path);

	last_partition_num = ped_disk_get_last_partition_num(disk);
	partition = ped_disk_get_partition(disk, last_partition_num);

	if (!partition) {
		LOG(MOD "Cannot get partition '%d' disk '%s'\n", last_partition_num, disk_path);
		return false;
	}

	start = partition->geom.start;
	end = (-PED_MEGABYTE_SIZE) / dev->sector_size + dev->length;

	geometry_start.dev = dev;
	geometry_start.start = start;
	geometry_start.end = end;
	geometry_start.length = 1;

	geometry_end = ped_geometry_new(dev, end, 1);

	if (!geometry_end) {
		LOG(MOD "Cannot get partition '%d' disk '%s'\n", last_partition_num, disk_path);
		return false;
	}

	constraint = ped_constraint_new(ped_alignment_any, ped_alignment_any, &geometry_start, geometry_end, 1, dev->length);

	if (!constraint) {
		LOG(MOD "Cannot create a new constraint disk '%s'\n", disk_path);
		goto fail1;
	}

	if (!ped_disk_set_partition_geom(disk, partition, constraint, start, end)) {
		LOG(MOD "Cannot set partition geometry disk '%s'\n", disk_path);
		goto fail2;
	}

	if (!ped_disk_commit(disk)) {
		LOG(MOD "Cannot write the partition table to disk '%s'\n", disk_path);
		goto fail2;
	}

	partition_path = ped_partition_get_path(partition);

	if (!partition_path) {
		LOG(MOD "Cannot get partition path disk '%s'\n", disk_path);
		goto fail2;
	}

	snprintf(command, LINE_MAX, RESIZEFS_PATH " %s", partition_path);
	async_task_exec(command);

	result = true;
	LOG(MOD "Resizing filesystem done\n");

fail2:
	ped_constraint_destroy (constraint);
fail1:
	ped_geometry_destroy (geometry_end);
	return result;
}

gboolean disk_by_label(const gchar* label, gchar** device) {
	const char* devpath = NULL;
	blkid_dev dev = NULL;
	blkid_cache cache = NULL;

	*device = NULL;

	if (blkid_get_cache(&cache, "/dev/null") != 0) {
		LOG(MOD "Cannot get cache!\n");
		return false;
	}

	if (blkid_probe_all(cache) != 0) {
		LOG(MOD "Probe all failed!\n");
		return false;
	}

	dev = blkid_find_dev_with_tag(cache, "LABEL", label);
	if (!dev) {
		LOG(MOD "Device wiht label '%s' not found!\n", label);
		return false;
	}

	devpath = blkid_dev_devname(dev);
	if (!devpath) {
		LOG(MOD "Cannot get device name!\n");
		return false;
	}

	*device = g_strdup(devpath);
	return true;
}

gboolean type_by_device(const gchar* device, gchar** type) {
	gboolean result = false;
	const char *devtype = NULL;
	blkid_probe probe = NULL;

	*type = NULL;

	probe = blkid_new_probe_from_filename(device);
	if(!probe) {
		LOG(MOD "Probe from filename failed!\n");
		return false;
	}
	if (blkid_probe_enable_partitions(probe, true) != 0) {
		LOG(MOD "Enable partitions failed!\n");
		goto fail;
	}
	if (blkid_do_fullprobe(probe) != 0) {
		LOG(MOD "Fullprobe failed!\n");
		goto fail;
	}
	if (blkid_probe_lookup_value(probe, "TYPE", &devtype, NULL) != 0) {
		LOG(MOD "Lookup value failed!\n");
		goto fail;
	}

	*type = g_strdup(devtype);
	result = true;
fail:
	blkid_free_probe(probe);
	return result;
}
