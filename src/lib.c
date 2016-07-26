/***
 Copyright © 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>
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
#include <time.h>
#include <sys/sendfile.h>
#include <libgen.h>
#include <sys/mount.h>
#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <linux/loop.h>

#include <glib.h>

#include "debug.h"
#include "lib.h"
#include "disk.h"

#define MOD "lib: "
#define BOOT_ID_SIZE 32
#define LOOP_MAJOR_ID 7
#define SUDOERS_PATH SYSCONFDIR "/sudoers.d/"
#define INSTANCE_ID_FILE DATADIR_PATH "/instance-id"
#define FIRST_BOOT_ID_FILE DATADIR_PATH "/first-boot-id"
#define KERNEL_BOOT_ID_FILE "/proc/sys/kernel/random/boot_id"

G_LOCK_DEFINE(first_boot_id_file);


void LOG(const char *fmt, ...) {
	va_list args;
	struct timespec now;

	va_start(args, fmt);
	clock_gettime(CLOCK_MONOTONIC, &now);
	fprintf(stderr, "[%f] ", ((double)now.tv_sec + ((double)now.tv_nsec / 1000000000.0)));
	vfprintf(stderr, fmt, args);

	va_end(args);
}

bool exec_task(const gchar* command_line) {
	gchar* standard_output = NULL;
	gchar* standard_error = NULL;
	gchar* cmd_line = NULL;
	GError* error = NULL;
	gint exit_status = 0;
	gboolean result;
	GString* command;
	command = g_string_new("");
	cmd_line = g_strescape(command_line, NULL);
	g_string_printf(command, SHELL_PATH " -c \"%s\"", cmd_line );
	g_free(cmd_line);

	LOG(MOD "Executing: %s\n", command->str);
	result = g_spawn_command_line_sync(command->str,
		&standard_output,
		&standard_error,
		&exit_status,
		&error);

	g_string_free(command, true);

	if (!result || exit_status != 0) {
		result = false;
		LOG(MOD "Command failed\n");
		if (error) {
			LOG(MOD "Error: %s\n", (char*)error->message);
		}
		if (standard_error) {
			LOG(MOD "STD Error: %s\n", (char*)standard_error);
		}
	}

	if (standard_output) {
		LOG(MOD "STD output: %s\n", (char*)standard_output);
		g_free(standard_output);
	}

	if (standard_error) {
		g_free(standard_error);
	}

	if (error) {
		g_error_free(error);
	}

	return result;
}

int make_dir(const char* pathname, mode_t mode) {
	struct stat stats;
	if (stat(pathname, &stats) != 0) {
		if (g_mkdir_with_parents(pathname, (gint)mode) != 0) {
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

bool write_file(const char* data, gsize data_len, const gchar* file_path, int oflags, mode_t mode) {
	int fd;
	bool result = true;

	fd = open(file_path, oflags, S_IWUSR);
	if (-1 == fd) {
		LOG(MOD "Cannot open %s\n", (char*)file_path);
		return false;
	}

	if (write(fd, data, data_len) == -1) {
		LOG(MOD "Cannot write in file '%s'", (char*)file_path);
		result = false;
	}

	if (fchmod(fd, mode) == -1) {
		LOG(MOD "Cannot change mode file '%s'", (char*)file_path);
		result = false;
	}

	if (close(fd) == -1) {
		LOG(MOD "Cannot close file '%s'", (char*)file_path);
		result = false;
	}

	return result;
}

int chown_path(const char* pathname, const char* ownername, const char* groupname) {
	struct passwd pwd;
	struct passwd* pwd_result;
	char* pwd_buf;
	long int pwd_bufsize;
	struct group grp;
	struct group* grp_result;
	char* grp_buf;
	long int grp_bufsize;
	int result = 1;

	pwd_bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (pwd_bufsize == -1) {
		pwd_bufsize = 1<<14;
	}

	pwd_buf = malloc((size_t)pwd_bufsize);
	if (pwd_buf == NULL) {
		LOG(MOD "Unable to allocate memory for passwd buffer\n");
		return 1;
	}

	getpwnam_r(ownername, &pwd, pwd_buf, (size_t)pwd_bufsize, &pwd_result);
	if (pwd_result == NULL) {
		LOG(MOD "User not found '%s'\n", ownername);
		goto fail1;
	}

	grp_bufsize = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (grp_bufsize == -1) {
		grp_bufsize = 1<<14;
	}

	grp_buf = malloc((size_t)grp_bufsize);
	if (grp_buf == NULL) {
		LOG(MOD "Unable to allocate memory for group buffer\n");
		goto fail1;
	}

	getgrnam_r(groupname, &grp, grp_buf, (size_t)grp_bufsize, &grp_result);
	if (grp_result == NULL) {
		LOG(MOD "Group not found '%s'\n", groupname);
		goto fail2;
	}

	result = chown(pathname, pwd.pw_uid, grp.gr_gid);

fail2:
	free(grp_buf);
fail1:
	free(pwd_buf);
	return result;
}

bool write_sudo_directives(const GString* data, const gchar* filename, int oflags) {
	gchar sudoers_file[PATH_MAX] = { 0 };
	g_strlcpy(sudoers_file, SUDOERS_PATH, PATH_MAX);
	if (make_dir(sudoers_file, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
		return false;
	}

	g_strlcat(sudoers_file, filename, PATH_MAX);

	return write_file(data->str, data->len, sudoers_file, oflags, S_IRUSR|S_IRGRP);
}

bool write_ssh_keys(const GString* data, const gchar* username) {
	int i;
	gchar auth_keys_file[PATH_MAX];
	gchar* auth_keys_content = NULL;
	gchar** vector_ssh_keys = NULL;
	GString* ssh_keys = NULL;
	struct passwd pwd;
	struct passwd* pwd_result;
	char* pwd_buf;
	long int pwd_bufsize;
	struct stat st;

	pwd_bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (pwd_bufsize == -1) {
		pwd_bufsize = 1<<14;
	}

	pwd_buf = malloc((size_t)pwd_bufsize);
	if (pwd_buf == NULL) {
		LOG(MOD "Unable to allocate memory for passwd buffer\n");
		return false;
	}

	getpwnam_r(username, &pwd, pwd_buf, (size_t)pwd_bufsize, &pwd_result);
	if (pwd_result == NULL) {
		LOG(MOD "User not found '%s'\n", username);
		free(pwd_buf);
		return false;
	}

	if (pwd.pw_dir) {
		g_snprintf(auth_keys_file, PATH_MAX, "%s/.ssh/", pwd.pw_dir);
		free(pwd_buf);

		if (make_dir(auth_keys_file, S_IRWXU) != 0) {
			LOG(MOD "Cannot create %s.\n", auth_keys_file);
			return false;
		}

		if (chown_path(auth_keys_file, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_file);
			return false;
		}

		g_strlcat(auth_keys_file, "authorized_keys", PATH_MAX);

		if (stat(auth_keys_file, &st) != 0) {
			if (!write_file(data->str, data->len, auth_keys_file, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)) {
				return false;
			}
		} else {
			if (!g_file_get_contents(auth_keys_file, &auth_keys_content, NULL, NULL)) {
				return false;
			}

			ssh_keys = g_string_new("");

			vector_ssh_keys = g_strsplit(data->str, "\n", -1);

			for (i=0; vector_ssh_keys[i]; ++i) {
				if (!g_strstr_len(auth_keys_content, -1, vector_ssh_keys[i])) {
					g_string_append_printf(ssh_keys, "%s\n", vector_ssh_keys[i]);
				}
			}

			g_free(auth_keys_content);
			g_strfreev(vector_ssh_keys);

			if (!write_file(ssh_keys->str, ssh_keys->len, auth_keys_file, O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR)) {
				g_string_free(ssh_keys, true);
				return false;
			}

			g_string_free(ssh_keys, true);
		}

		if (chown_path(auth_keys_file, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_file);
			return false;
		}
	}

	return true;
}

bool copy_file(const gchar* src, const gchar* dest) {
	int fd_src = 0;
	int fd_dest = 0;
	struct stat st = { 0 };
	ssize_t send_result = 0;
	bool result = false;
	off_t bytes_copied = 0;
	gchar dest_dir[PATH_MAX] = { 0 };

	fd_src = open(src, O_RDONLY);
	if (-1 == fd_src) {
		LOG(MOD "Unable to open source file '%s'\n", src);
		return false;
	}

	if (fstat(fd_src, &st) == -1) {
		LOG(MOD "Unable to get info from file '%s'\n", src);
		goto fail1;
	}

	g_strlcpy(dest_dir, dest, PATH_MAX);
	if (make_dir(dirname(dest_dir), st.st_mode) != 0) {
		LOG(MOD "Unable to create directory '%s'\n", dest_dir);
		goto fail1;
	}

	fd_dest = open(dest, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR);
	if (-1 == fd_dest) {
		LOG(MOD "Unable to open destination file '%s'\n", dest);
		goto fail1;
	}

	send_result = sendfile(fd_dest, fd_src, &bytes_copied, (size_t)st.st_size);
	if (-1 == send_result) {
		LOG(MOD "Unable to copy file from '%s' to '%s'\n", src, dest);
		goto fail2;
	}

	if (fchmod(fd_dest, st.st_mode) != 0) {
		LOG(MOD "Unable to chmod '%d' '%s'\n", st.st_mode, dest);
		goto fail2;
	}

	result = true;

fail2:
	close(fd_dest);
fail1:
	close(fd_src);
	return result;
}

bool mount_filesystem(const gchar* device, const gchar* mountdir, gchar** loop_device_) {
	struct stat st = { 0 };
	gchar* devtype = NULL;
	bool result = false;
	int loopfd = -1;
	int filefd = -1;
	unsigned int minor_id;
	dev_t device_id = 0;
	struct loop_info64 li64 = { 0 };
	struct loop_info li32 = { 0 };
	gchar loop_device[PATH_MAX] = { 0 };

	if (stat(device, &st)) {
		LOG(MOD "stat failed\n");
		return false;
	}

	if (!type_by_device(device, &devtype)) {
		LOG("Unknown filesystem device '%s'\n", device);
		return false;
	}

	/* block device */
	if ((st.st_mode & S_IFMT) == S_IFBLK) {
		if (mount(device, mountdir, devtype, MS_NODEV|MS_NOEXEC|MS_RDONLY, NULL) != 0) {
			LOG(MOD "Unable to mount config drive '%s'\n", device);
			free(devtype);
			return false;
		}
		free(devtype);
		return true;
	}

	/* loking for minor device id */
	for (minor_id=0; minor_id<=LOOP_MAJOR_ID; ++minor_id) {
		g_snprintf(loop_device, PATH_MAX, "/dev/loop%u", minor_id);
		if (stat(loop_device, &st) == -1) {
			break;
		}
		loop_device[0] = 0;
	}

	if (!loop_device[0]) {
		LOG(MOD "Cannot find any free loop device\n");
		goto fail1;
	}

	device_id = makedev(LOOP_MAJOR_ID, minor_id);

	if (mknod(loop_device, S_IFBLK, device_id) != 0) {
		LOG(MOD "Unable to create loop device '%s'\n", loop_device);
		goto fail1;
	}

	loopfd = open(loop_device, O_RDONLY);
	if (loopfd < 0) {
		LOG(MOD "Open loop device failed '%s'\n", loop_device);
		remove(loop_device);
		goto fail1;
	}

	filefd = open(device, O_RDONLY);
	if (filefd < 0) {
		LOG(MOD "Open filesystem failed '%s'\n", device);
		goto fail2;
	}

	snprintf((char*)li64.lo_file_name, LO_NAME_SIZE, "%s", device);
	li64.lo_offset = 0;
	li64.lo_encrypt_key_size = 0;

	if (ioctl(loopfd, LOOP_SET_FD, filefd) < 0) {
		LOG(MOD "Set fd failed\n");
		goto fail3;
	}
	close(filefd);
	filefd = 0;

	if (ioctl(loopfd, LOOP_SET_STATUS64, &li64) < 0) {
		snprintf(li32.lo_name, LO_NAME_SIZE, "%s", device);
		li32.lo_offset = 0;
		li32.lo_encrypt_key_size = 0;

		if (ioctl(loopfd, LOOP_SET_STATUS, &li32) < 0) {
			LOG(MOD "Unable to set loop status '%s'\n", loop_device);
			ioctl(loopfd, LOOP_CLR_FD, 0);
			remove(loop_device);
			goto fail3;
		}
	}

	if (make_dir(mountdir, S_IRUSR) != 0) {
		LOG(MOD "Unable to create mount dir '%s'\n", mountdir);
		goto fail3;
	}

	if (mount(loop_device, mountdir, devtype, MS_NODEV|MS_NOEXEC|MS_RDONLY, NULL) != 0) {
		LOG(MOD "Unable to mount config drive '%s'\n", device);
		goto fail3;
	}

	*loop_device_ = g_malloc(sizeof(gchar)*PATH_MAX);
	g_strlcpy(*loop_device_, loop_device, PATH_MAX);

	result = true;

fail3:
	if (filefd) {
		close(filefd);
	}
fail2:
	if (loopfd) {
		close(loopfd);
	}
fail1:
	g_free(devtype);
	return result;
}

bool umount_filesystem(const gchar* mountdir, const gchar* loop_device) {
	int fd = -1;

	if (loop_device) {
		fd = open(loop_device, O_RDONLY);
		if (fd < 0) {
			LOG(MOD "Unable to open loop device '%s'\n", loop_device);
			return false;
		}

		if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
			LOG(MOD "Clear fd failed\n");
			close(fd);
			return false;
		}

		close(fd);
		remove(loop_device);
	}

	if (umount(mountdir) != 0) {
		LOG(MOD "Cannot umount '%s'\n", mountdir);
		return false;
	}

	remove(mountdir);
	return true;
}

bool save_instance_id(const gchar* instance_id) {
	bool result = false;
	gchar* last_instance_id = NULL;
	struct stat st;

	if (stat(INSTANCE_ID_FILE, &st) != 0) {
		if (!write_file(instance_id, strlen(instance_id), INSTANCE_ID_FILE, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR)) {
			LOG(MOD "Unable to save instance id\n");
			goto exit;
		}
	} else {
		if (!g_file_get_contents(INSTANCE_ID_FILE, &last_instance_id, NULL, NULL)) {
			LOG(MOD "Unable to read file '%s'\n", INSTANCE_ID_FILE);
			goto exit;
		}
		if (g_strcmp0(instance_id, last_instance_id) != 0) {
			g_free(last_instance_id);
			if (!write_file(instance_id, strlen(instance_id), INSTANCE_ID_FILE, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR)) {
				LOG(MOD "Unable to save instance id\n");
				goto exit;
			}
			G_LOCK(first_boot_id_file);
			remove(FIRST_BOOT_ID_FILE);
			G_UNLOCK(first_boot_id_file);
		}
	}

	result = true;

exit:
	return result;
}

bool is_first_boot(void) {
	struct stat st;
	gchar* boot_id = NULL;
	gchar* first_boot_id = NULL;
	static int cache_firstboot = -1;

	G_LOCK(first_boot_id_file);
	if (stat(FIRST_BOOT_ID_FILE, &st) != 0) {
		cache_firstboot = 1;
		boot_id = get_boot_id();
		if (!boot_id || !write_file(boot_id, strlen(boot_id), FIRST_BOOT_ID_FILE, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR)) {
			LOG(MOD "Unable to save boot id\n");
			goto exit;
		}
	} else {
		if (cache_firstboot != -1) {
			goto exit;
		}
		if (!g_file_get_contents(FIRST_BOOT_ID_FILE, &first_boot_id, NULL, NULL)) {
			LOG(MOD "Unable to read file '%s'\n", FIRST_BOOT_ID_FILE);
			goto exit;
		}
		boot_id = get_boot_id();
		if (boot_id && g_strcmp0(first_boot_id, boot_id) == 0) {
			cache_firstboot = 1;
		} else {
			cache_firstboot = 0;
		}
	}

exit:
	G_UNLOCK(first_boot_id_file);
	if (boot_id) {
		g_free(boot_id);
	}
	if (first_boot_id) {
		g_free(first_boot_id);
	}
	LOG(MOD "First boot: %s\n", (cache_firstboot != 1 ? "False" : "True" ));
	return (cache_firstboot != 1 ? false : true);
}

bool gnode_free(GNode* node, __unused__ gpointer data) {
	if (node->data) {
		g_free(node->data);
	}
	return false;
}

char* get_boot_id(void) {
	char *boot_id = NULL;
	static char cache_boot_id[BOOT_ID_SIZE] = { 0 };

	if (cache_boot_id[0]) {
		return g_strdup(cache_boot_id);
	}

	if (!g_file_get_contents(KERNEL_BOOT_ID_FILE, &boot_id, NULL, NULL)) {
		LOG(MOD "Unable to get boot id from '%s'\n", KERNEL_BOOT_ID_FILE);
		return NULL;
	}

	g_strlcpy(cache_boot_id, boot_id, BOOT_ID_SIZE);

	return boot_id;
}
