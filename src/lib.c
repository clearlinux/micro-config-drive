/***
 Copyright (C) 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>
 Author: Julio Montes <julio.montes@intel.com>

 This file is part of clr-cloud-init.

 clr-cloud-init is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 clr-cloud-init is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with clr-cloud-init. If not, see <http://www.gnu.org/licenses/>.

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

/*
 * lib.c - collection of misc functions for modules to do work
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <glib.h>

#include "debug.h"

#define MOD "lib: "

void LOG(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void exec_task(const gchar* task) {
	/*
	 * execute a process externally in a secured and limited enviroment,
	 * handle return values gracefully
	 */
	LOG(MOD "Executing: %s\n", task);
	system(task);
}

int make_dir(const char* pathname, mode_t mode) {
	struct stat stats;
	if (stat(pathname, &stats) != 0) {
		if (mkdir(pathname, mode) != 0) {
			LOG(MOD "Cannot create directory %s\n", pathname);
			return -1;
		}
	} else if (!S_ISDIR (stats.st_mode)) {
		LOG(MOD "%s already exists and is not a directory.\n",
			pathname);
		return -1;
	}
	return 0;
}

int chown_path(const char* pathname, const char* ownername, const char* groupname) {
	uid_t owner_id;
	gid_t group_id;
	struct passwd *pw;
	struct group *grp;

	pw = getpwnam(ownername);
	owner_id = pw ? pw->pw_uid : (uid_t) - 1;
	grp = getgrnam(groupname);
	group_id = grp ? grp->gr_gid : (gid_t) - 1;

	return chown(pathname, owner_id, group_id);
}

int write_sudo_string(const gchar* filename, const gchar* data) {
	int fd;
	gchar sudoers_file[PATH_MAX];
	g_snprintf(sudoers_file, PATH_MAX, "/etc/sudoers.d/");
	if (make_dir(sudoers_file, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
		return 1;
	}

	g_strlcat(sudoers_file, filename, PATH_MAX);
	fd = open(sudoers_file, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IRGRP);
	if (-1 == fd) {
		LOG(MOD "Cannot open %s\n", sudoers_file);
		return 1;
	}

	write(fd, data, strlen(data));
	write(fd, "\n", 1);

	close(fd);

	return 0;
}

int write_ssh_key(const gchar* ssh_key, const gchar* username) {
	int fd;
	gchar auth_keys_path[PATH_MAX];
	struct passwd *pw;

	pw = getpwnam(username);

	if (pw && pw->pw_dir) {
		g_snprintf(auth_keys_path, PATH_MAX, "%s/.ssh", pw->pw_dir);

		if (make_dir(auth_keys_path, S_IRWXU) != 0) {
			LOG(MOD "Cannot create %s.\n", auth_keys_path);
			return 1;
		}

		if (chown_path(auth_keys_path, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_path);
			return 1;
		}

		g_strlcat(auth_keys_path, "/authorized_keys", PATH_MAX);
		fd = open(auth_keys_path, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR);
		if (-1 == fd) {
			LOG(MOD "Cannot open %s.\n", auth_keys_path);
			return 1;
		}

		LOG(MOD "Using %s\n", auth_keys_path);
		LOG(MOD "Writing %s\n", ssh_key);
		write(fd, ssh_key, strlen(ssh_key));
		write(fd, "\n", 1);
		close(fd);

		if (chown_path(auth_keys_path, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_path);
			return 1;
		}
	}
	return 0;
}

/*
* Get the partition of a mount point
* On Success the function return true,
* otherwise false is returned
*/
gboolean get_partition(const gchar* mountpoint, gchar* partition, guint partition_len) {
	FILE* file;
	gchar buffer[LINE_MAX];
	char *s;
	char *token;
	char major_minor_mountpoint[32] = { 0 };
	char major_minor_partition[32] = { 0 };
	gboolean mountpoint_found = false;
	gboolean partition_found = false;

	file = fopen("/proc/self/mountinfo", "r");
	if (!file) {
		LOG(MOD "Error opening /proc/self/mountinfo\n");
		return false;
	}

	while (fgets(buffer, LINE_MAX, file)) {
		/* ignore first two tokens */
		strtok_r(buffer, "\n\t ", &s);
		strtok_r(NULL, "\n\t ", &s);
		/* major and minor, i.e 0:23 */
		token = strtok_r(NULL, "\n\t ", &s);
		if (!token) {
			continue;
		}
		g_strlcpy(major_minor_mountpoint, token, sizeof(major_minor_mountpoint));
		/*ignore separator / */
		strtok_r(NULL, "\n\t ", &s);
		/* mount point, i.e / */
		token = strtok_r(NULL, "\n\t ", &s);
		if (g_strcmp0(mountpoint, token) == 0) {
			mountpoint_found = true;
			break;
		}
	}

	fclose(file);

	if (!mountpoint_found) {
		LOG(MOD "Failed to find mount point '%s'\n", mountpoint);
		return false;
	}

	/* next step, looking for the partition with major_minor_mountpoint */
	file = fopen("/proc/diskstats", "r");
	if (file == NULL) {
		LOG(MOD "Error opening /proc/diskstats\n");
		return false;
	}

	while (fgets(buffer, LINE_MAX, file) != NULL) {
		/* major */
		token = strtok_r(buffer, "\n\t ", &s);
		if (!token) {
			continue;
		}
		g_snprintf(major_minor_partition, sizeof(major_minor_partition), "%s:", token);
		/* minor */
		token = strtok_r(NULL, "\n\t ", &s);
		if (!token) {
			continue;
		}
		g_strlcat(major_minor_partition, token, sizeof(major_minor_partition));
		if (g_strcmp0(major_minor_mountpoint, major_minor_partition) == 0) {
			/* partition, i.e vda2 */
			token = strtok_r(NULL, "\n\t ", &s);
			memset(partition, 0, partition_len);
			snprintf(partition, partition_len, "/dev/%s", token);
			partition_found = true;
			break;
		}
	}

	fclose(file);

	if (!partition_found) {
		LOG(MOD "Failed to find partition '%s'\n", major_minor_mountpoint);
		return false;
	}

	return true;
}
