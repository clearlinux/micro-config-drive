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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/sysinfo.h>

#include <glib.h>

#include "handlers.h"
#include "disk.h"
#include "lib.h"
#include "userdata.h"
#include "datasources.h"
#include "default_user.h"
#include "openstack.h"


/* Long options */
enum {
	OPT_OPENSTACK_METADATA_FILE=1001,
	OPT_OPENSTACK_CONFIG_DRIVE,
	OPT_USER_DATA,
	OPT_METADATA,
	OPT_NO_GROWPART,
};

/* supported datasources */
enum {
	DS_OPENSTACK=500,
};

static struct option opts[] = {
	{ "user-data-file",             required_argument, NULL, 'u' },
	{ "openstack-metadata-file",    required_argument, NULL, OPT_OPENSTACK_METADATA_FILE },
	{ "openstack-config-drive",     required_argument, NULL, OPT_OPENSTACK_CONFIG_DRIVE },
	{ "user-data",                  no_argument, NULL, OPT_USER_DATA },
	{ "metadata",                   no_argument, NULL, OPT_METADATA },
	{ "help",                       no_argument, NULL, 'h' },
	{ "version",                    no_argument, NULL, 'v' },
	{ "first-boot",                 no_argument, NULL, 'b' },
	{ "no-growpart",                no_argument, NULL, OPT_NO_GROWPART },
	{ NULL, 0, NULL, 0 }
};

typedef bool (*async_task_function) (gpointer);

static struct async_data {
	GMainLoop* main_loop;
	guint remaining;
} async_data;

static void async_process_watcher(GPid pid, gint status, gpointer _data) {
	struct async_data* data = (struct async_data*)_data;

	LOG("PID %d ends, exit status %d\n", pid, status);

	--(data->remaining);

	if (0 == data->remaining) {
		LOG("Quit main loop\n");
		g_main_loop_quit(data->main_loop);
	}

	g_spawn_close_pid(pid);
}

bool async_checkdisk(gpointer data) {
	gchar* root_disk;

	root_disk = disk_for_path("/");
	if (root_disk) {
		LOG("Checking disk %s\n", root_disk);
		if (!disk_resize_grow(root_disk, async_process_watcher, data)) {
			return false;
		}
	} else {
		LOG("Root disk not found\n");
		return false;
	}
	return true;
}

bool async_setup_first_boot(gpointer data) {
	gchar command[LINE_MAX] = { 0 };
	GString* sudo_directives = NULL;

	/* default user will be able to use sudo */
	sudo_directives = g_string_new(DEFAULT_USER_SUDO);
	if (!write_sudo_directives(sudo_directives, DEFAULT_USER_USERNAME"-cloud-init")) {
		LOG("Failed to enable sudo rule for user: %s\n", DEFAULT_USER_SUDO);
	}
	g_string_free(sudo_directives, true);

	/* lock root account for security */
	g_snprintf(command, LINE_MAX, USERMOD_PATH " -p '!' root");
	return exec_task_async(command, async_process_watcher, data);
}

static void run_task(gpointer function, gpointer data) {
	async_task_function func = *(async_task_function*)(&function);
	if ( ! func(data) ) {
		async_process_watcher(0, 0, data);
	}
}

static void async_item(gpointer function, gpointer thread_pool) {
	GError *error = NULL;

	++(async_data.remaining);

	g_thread_pool_push((GThreadPool*)thread_pool, function, &error);
	if (error) {
		LOG("Error pushing a new thread: %s\n", (char*)error->message);
		g_error_free(error);
	}
}

static gint run_async_tasks(GPtrArray* async_tasks_array) {
	gint result_code = EXIT_FAILURE;
	GThreadPool* thread_pool = NULL;

	async_data.main_loop = g_main_loop_new(NULL, 0);
	if (!async_data.main_loop) {
		LOG("Cannot create a new main loop\n");
		goto fail1;
	}

	thread_pool = g_thread_pool_new(run_task, &async_data, get_nprocs(), true, NULL);
	if (!thread_pool) {
		LOG("Cannot create a new thread pool\n");
		goto fail2;
	}

	//push threads to pool
	g_ptr_array_foreach(async_tasks_array, async_item, thread_pool);

	//run main loop to wait the end of async tasks
	g_main_loop_run(async_data.main_loop);

	result_code = EXIT_SUCCESS;

	g_thread_pool_free(thread_pool, false, true);

fail2:
	g_main_loop_unref(async_data.main_loop);
fail1:
	return result_code;
}

int main(int argc, char *argv[]) {
	int c;
	int i;
	unsigned int datasource = 0;
	int result_code = EXIT_SUCCESS;
	bool no_growpart = false;
	bool first_boot = false;
	bool data_processed = false;
	struct datasource_options_struct datasource_opts = { 0 };
	char* userdata_filename = NULL;
	char* tmp_metadata_filename = NULL;
	char* tmp_data_filesystem = NULL;
	char metadata_filename[PATH_MAX] = { 0 };
	char data_filesystem_path[PATH_MAX] = { 0 };
	GError* error = NULL;
	GThread* async_tasks_thread = NULL;
	async_task_function func = NULL;
	gchar command[LINE_MAX] = { 0 };
	GPtrArray* async_tasks_array = NULL;

	while (true) {
		c = getopt_long(argc, argv, "u:hvb", opts, &i);

		if (c == -1) {
			break;
		}

		switch (c) {

		case 'u':
			userdata_filename = realpath(optarg, optarg);
			if (!userdata_filename) {
				LOG("Userdata file not found '%s'\n", optarg);
			}
			break;

		case 'h':
			LOG("Usage: %s [options]\n", argv[0]);
			LOG("-u, --user-data-file [file]            specify a custom user data file\n");
			LOG("    --openstack-metadata-file [file]   specify an Openstack metadata file\n");
			LOG("    --openstack-config-drive [path]    specify an Openstack config drive to process\n");
			LOG("                                       metadata and user data (iso9660 or vfat filesystem)\n");
			LOG("    --user-data                        get and process user data from data sources\n");
			LOG("    --metadata                         get and process metadata from data sources\n");
			LOG("-h, --help                             display this help message\n");
			LOG("-v, --version                          display the version number of this program\n");
			LOG("-b, --first-boot                       set up the system in its first boot\n");
			LOG("    --no-growpart                      do not verify disk partitions.\n");
			LOG("                                       %s will not resize the filesystem\n", argv[0]);
			exit(EXIT_SUCCESS);
			break;

		case 'v':
			fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
			exit(EXIT_SUCCESS);
			break;

		case 'b':
			first_boot = true;
			break;

		case '?':
			exit(EXIT_FAILURE);
			break;

		case OPT_OPENSTACK_METADATA_FILE:
			datasource = DS_OPENSTACK;
			tmp_metadata_filename = strdup(optarg);
			break;

		case OPT_OPENSTACK_CONFIG_DRIVE:
			datasource = DS_OPENSTACK;
			tmp_data_filesystem = strdup(optarg);
			break;

		case OPT_USER_DATA:
			datasource_opts.user_data = true;
			break;

		case OPT_METADATA:
			datasource_opts.metadata = true;
			break;

		case OPT_NO_GROWPART:
			no_growpart = true;
			break;
		}
	}

#ifdef HAVE_CONFIG_H
	LOG("clr-cloud-init version: %s\n", PACKAGE_VERSION);
#endif /* HAVE_CONFIG_H */

	async_tasks_array = g_ptr_array_new();
	if (!async_tasks_array) {
		LOG("Unable to create a new ptr array for async tasks\n");
	}

	/* at one point in time this should likely be a fatal error */
	if (geteuid() != 0) {
		LOG("%s isn't running as root, this will most likely fail!\n", argv[0]);
	}

	if (!no_growpart && async_tasks_array) {
		func = async_checkdisk;
		g_ptr_array_add(async_tasks_array, *(async_task_function**)&func);
	}

	if (first_boot && async_tasks_array) {
		func = async_setup_first_boot;
		g_ptr_array_add(async_tasks_array, *(async_task_function**)&func);
	}

	if (async_tasks_array && async_tasks_array->len > 0) {
		if (get_nprocs() > 1) {
			async_tasks_thread = g_thread_try_new("run_async_tasks", (GThreadFunc)run_async_tasks,
								async_tasks_array, &error);
			if (!async_tasks_thread) {
				LOG("Cannot create a thread to run async tasks!");
				if (error) {
					LOG("Error: %s\n", (char*)error->message);
					g_error_free(error);
					error = NULL;
				}
			}
		} else {
			run_async_tasks(async_tasks_array);
		}
	}

	if (first_boot) {
		/* default user will be used by ccmodules and datasources */
		g_snprintf(command, LINE_MAX, USERADD_PATH
				" -U -d '%s' -G '%s' -f '%s' -e '%s' -s '%s' -c '%s' -p '%s' '%s'"
				, DEFAULT_USER_HOME_DIR
				, DEFAULT_USER_GROUPS
				, DEFAULT_USER_INACTIVE
				, DEFAULT_USER_EXPIREDATE
				, DEFAULT_USER_SHELL
				, DEFAULT_USER_GECOS
				, DEFAULT_USER_PASSWORD
				, DEFAULT_USER_USERNAME);
		exec_task(command);
	}

	/* process metadata file */
	if (tmp_metadata_filename) {
		if (realpath(tmp_metadata_filename, metadata_filename)) {
			switch (datasource) {
			case DS_OPENSTACK:
				if (!openstack_process_metadata(metadata_filename)) {
					result_code = EXIT_FAILURE;
				}
				break;
			default:
				LOG("Unsupported datasource '%d'\n", datasource);
			}
		} else {
			LOG("Metadata file not found '%s'\n", tmp_metadata_filename);
		}

		free(tmp_metadata_filename);
		tmp_metadata_filename = NULL;
	}

	/* process userdata/metadata using iso9660 or vfat filesystem */
	if (tmp_data_filesystem) {
		if (realpath(tmp_data_filesystem, data_filesystem_path)) {
			switch (datasource) {
			case DS_OPENSTACK:
				if (!openstack_process_config_drive(data_filesystem_path)) {
					result_code = EXIT_FAILURE;
				}
			break;
			default:
				LOG("Unsupported datasource '%d'\n", datasource);
			}
		} else {
			LOG("iso9660 or vfat filesystem not found '%s'\n", tmp_data_filesystem);
		}

		free(tmp_data_filesystem);
		tmp_data_filesystem = NULL;
	}

	/* process userdata file */
	if (userdata_filename) {
		if (!userdata_process_file(userdata_filename)) {
			result_code = EXIT_FAILURE;
		}

		free(userdata_filename);
		userdata_filename = NULL;
	}

	if (datasource_opts.user_data || datasource_opts.metadata) {
		/* get/process userdata and metadata from datasources */
		for (i = 0; cloud_structs[i] != NULL; ++i) {
			if (EXIT_SUCCESS == cloud_structs[i]->handler(&datasource_opts)) {
				data_processed = true;
				break;
			}
		}

		if (!data_processed) {
			result_code = EXIT_FAILURE;
		}
	}

	if (async_tasks_thread) {
		g_thread_join(async_tasks_thread);
	}

	g_ptr_array_unref(async_tasks_array);

	exit(result_code);
}
