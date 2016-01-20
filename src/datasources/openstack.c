/***
 Copyright (C) 2015 Intel Corporation

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

#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <glib.h>
#include <json-glib/json-glib.h>

#include "openstack.h"
#include "handlers.h"
#include "curl.h"
#include "lib.h"
#include "userdata.h"
#include "json.h"
#include "default_user.h"
#include "disk.h"

#define MOD "openstack: "
#define OPENSTACK_URL "http://169.254.169.254/openstack"
#define USERDATA_URL OPENSTACK_URL "/latest/user_data"
#define METADATA_URL OPENSTACK_URL "/latest/meta_data.json"
#define USERDATA_DRIVE_PATH "/openstack/latest/user_data"
#define METADATA_DRIVE_PATH "/openstack/latest/meta_data.json"
#define ATTEMPTS 10
#define U_SLEEP 300000

int openstack_main(struct datasource_options_struct* opts);

static gboolean openstack_use_metadata_service(void);
static gboolean openstack_use_config_drive(void);

static void openstack_run_handler(GNode *node, __unused__ gpointer user_data);
static void openstack_item(GNode* node, GThreadPool* thread_pool);
static gboolean openstack_node_free(GNode* node, gpointer data);

static void openstack_metadata_not_implemented(GNode* node);
static void openstack_metadata_keys(GNode* node);
static void openstack_metadata_hostname(GNode* node);
static void openstack_metadata_files(GNode* node);

static struct datasource_options_struct* options = NULL;

enum {
	SOURCE_CONFIG_DRIVE = 101,
	SOURCE_METADATA_SERVICE,
	SOURCE_NONE
};

struct openstack_metadata_data {
	const gchar* key;
	void (*func)(GNode* node);
};

static struct openstack_metadata_data openstack_metadata_options[] = {
	{ "random_seed",        openstack_metadata_not_implemented },
	{ "uuid",               openstack_metadata_not_implemented },
	{ "availability_zone",  openstack_metadata_not_implemented },
	{ "keys",               openstack_metadata_keys            },
	{ "hostname",           openstack_metadata_hostname        },
	{ "launch_index",       openstack_metadata_not_implemented },
	{ "public_keys",        openstack_metadata_not_implemented },
	{ "project_id",         openstack_metadata_not_implemented },
	{ "name",               openstack_metadata_not_implemented },
	{ "files",              openstack_metadata_files           },
	{ "meta",               openstack_metadata_not_implemented },
	{ NULL }
};

struct datasource_handler_struct openstack_datasource = {
	.datasource="openstack",
	.handler=&openstack_main
};

static int data_source = SOURCE_NONE;

static CURL* curl = NULL;

static char config_drive_path[] = "/tmp/config-2-XXXXXX";

int openstack_main(struct datasource_options_struct* opts) {
	options = opts;
	data_source = SOURCE_NONE;

	if (openstack_use_config_drive()) {
		LOG(MOD "Metadata and userdata were processed using config drive\n");
		return EXIT_SUCCESS;
	} else if (openstack_use_metadata_service()) {
		LOG(MOD "Metadata and userdata were processed using metadata service\n");
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

gboolean openstack_process_config_drive(const gchar* path) {
	char userdata_file[] = "/tmp/userdata-XXXXXX";
	int fd_tmp = 0;
	gchar metadata_drive_path[PATH_MAX] = { 0 };
	gchar userdata_drive_path[PATH_MAX] = { 0 };
	gchar* devtype = NULL;
	gboolean result = false;
	struct stat st = { 0 };
	gchar command[PATH_MAX] = { 0 };
	gboolean process_user_data = true;
	gboolean process_metadata = true;

	if (options) {
		process_user_data = options->user_data;
		process_metadata = options->metadata;
	}

	if (!process_user_data && !process_metadata) {
		/*nothing to do*/
		return true;
	}

	data_source = SOURCE_CONFIG_DRIVE;

	if (!type_by_device(path, &devtype)) {
		LOG("Unknown filesystem device '%s'\n", (char*)path);
		return false;
	}

	if (!mkdtemp(config_drive_path)) {
		LOG(MOD "Unable to create directory '%s'\n", config_drive_path);
		goto fail1;
	}

	if (stat(path, &st)) {
		LOG(MOD "stat failed\n");
		goto fail1;
	}

	if ((st.st_mode & S_IFMT) != S_IFBLK) {
		g_snprintf(command, PATH_MAX, "mount -o loop,ro -t %s %s %s", devtype, path, config_drive_path );
		if (!exec_task(command)) {
			LOG(MOD "Unable to mount config drive '%s'\n", (char*)path);
			goto fail2;
		}
	} else if (mount(path, config_drive_path, devtype, MS_NODEV|MS_NOEXEC|MS_RDONLY, NULL) != 0) {
		LOG(MOD "Unable to mount config drive '%s'\n", (char*)path);
		goto fail2;
	}

	if (process_metadata) {
		g_snprintf(metadata_drive_path, PATH_MAX, "%s%s", config_drive_path, METADATA_DRIVE_PATH);
		if (!openstack_process_metadata_file(metadata_drive_path)) {
			LOG(MOD "Using config drive get and process metadata failed\n");
			goto fail3;
		}
	}

	result = true;

	if (process_user_data) {
		fd_tmp = mkstemp(userdata_file);
		if (-1 == fd_tmp) {
			LOG(MOD "Unable to create a temporal file\n");
			goto fail3;
		}
		if (close(fd_tmp) == -1) {
			LOG(MOD "Close file '%s' failed\n", userdata_file);
			goto fail4;
		}

		g_snprintf(userdata_drive_path, PATH_MAX, "%s%s", config_drive_path, USERDATA_DRIVE_PATH);

		if (!copy_file(userdata_drive_path, userdata_file)) {
			LOG(MOD "Copy file '%s' failed\n", userdata_drive_path);
			goto fail4;
		}

		if (!userdata_process_file(userdata_file)) {
			LOG(MOD "Unable to process userdata\n");
		}
	}

fail4:
	remove(userdata_file);
fail3:
	if ((st.st_mode & S_IFMT) != S_IFBLK) {
		g_snprintf(command, PATH_MAX, "umount %s", config_drive_path );
		if (!exec_task(command)) {
			LOG(MOD "Using config drive cannot umount '%s'\n", config_drive_path);
			goto fail2;
		}
	} else if (umount(config_drive_path) != 0) {
		LOG(MOD "Using config drive cannot umount '%s'\n", config_drive_path);
	}
fail2:
	rmdir(config_drive_path);
fail1:
	g_free(devtype);
	return result;
}

gboolean openstack_process_metadata_file(const gchar* filename) {
	GError* error = NULL;
	JsonParser* parser = NULL;
	GNode* node = NULL;
	gboolean result = false;
	GThreadPool* thread_pool = NULL;

	parser = json_parser_new();
	json_parser_load_from_file(parser, filename, &error);
	if (error) {
		LOG(MOD "Unable to parse '%s': %s\n", filename, error->message);
		g_error_free(error);
		goto fail0;
	}

	node = g_node_new(g_strdup(filename));
	json_parse(json_parser_get_root(parser), node, false);
	cloud_config_dump(node);

	thread_pool = g_thread_pool_new((GFunc)openstack_run_handler, NULL, get_nprocs(), true, NULL);

	if (!thread_pool) {
		LOG(MOD "Cannot create thread pool\n");
		goto fail1;
	}

	g_node_children_foreach(node, G_TRAVERSE_ALL, (GNodeForeachFunc)openstack_item, thread_pool);
	g_thread_pool_free(thread_pool, false, true);

	result = true;

fail1:
	g_node_traverse(node, G_POST_ORDER, G_TRAVERSE_ALL, -1, openstack_node_free, NULL);
fail0:
	g_object_unref(parser);
	g_node_destroy(node);
	return result;
}


static gboolean openstack_use_metadata_service(void) {
	gboolean result = false;
	gchar* data_filename = NULL;
	int attempts = ATTEMPTS;
	useconds_t u_sleep = U_SLEEP;

	data_source = SOURCE_METADATA_SERVICE;

	if (!curl_common_init(&curl)) {
		LOG(MOD "Curl initialize failed\n");
		goto fail1;
	}

	if (options->metadata) {
		LOG(MOD "Fetching metadata file URL %s\n", METADATA_URL );
		data_filename = curl_fetch_file(curl, METADATA_URL, attempts, u_sleep);
		if (!data_filename) {
			LOG(MOD "Fetch metadata failed\n");
			goto fail1;
		}

		result = openstack_process_metadata_file(data_filename);
		g_free(data_filename);
		data_filename = NULL;

		if (!result) {
			LOG(MOD "Process metadata failed\n");
			goto fail1;
		}

		/*
		* if metadata was downloaded, then we do not need to wait
		* for nova metadata service because at this point it is
		* already up (running)
		*/
		attempts = 1;
		u_sleep = 0;
	}

	/*
	* Get and process userdata is optional
	*/
	result = true;

	if (options->user_data) {
		LOG(MOD "Fetching userdata file URL %s\n", USERDATA_URL );
		data_filename = curl_fetch_file(curl, USERDATA_URL, attempts, u_sleep);
		if (!data_filename) {
			LOG(MOD "Fetch userdata failed\n");
			goto fail1;
		}

		result = userdata_process_file(data_filename);
		g_free(data_filename);
		data_filename = NULL;

		if (!result) {
			LOG(MOD "Process userdata failed\n");
			goto fail1;
		}
	}

fail1:
	curl_easy_cleanup(curl);
	return result;
}

static gboolean openstack_use_config_drive(void) {
	gboolean config_drive = false;
	gchar* device = NULL;
	gboolean result = false;

	config_drive = disk_by_label("config-2", &device);

	if (!config_drive) {
		LOG(MOD "Config drive not found\n");
		return false;
	}

	if (!openstack_process_config_drive(device)) {
		LOG(MOD "Process config drive failed\n");
		goto fail1;
	}

	result = true;

fail1:
	g_free(device);
	return result;
}

static gboolean openstack_node_free(GNode* node, __unused__ gpointer data) {
	if (node->data) {
		g_free(node->data);
	}

	return false;
}

static void openstack_run_handler(GNode *node, __unused__ gpointer user_data) {
	size_t i;

	if (node->data) {
		for (i = 0; openstack_metadata_options[i].key != NULL; ++i) {
			if (g_strcmp0(node->data, openstack_metadata_options[i].key) == 0) {
				LOG(MOD "Metadata using '%s' handler\n", (char*)node->data);
				openstack_metadata_options[i].func(node->children);
				return;
			}
		}
		LOG(MOD "Metadata no handler for '%s'.\n", (char*)node->data);
	}
}

static void openstack_item(GNode* node, GThreadPool* thread_pool) {
	GError *error = NULL;

	g_thread_pool_push(thread_pool, node, &error);

	if (error) {
		LOG(MOD "Cannot push a new thread: %s\n", (char*)error->message);
		g_error_free(error);
		error = NULL;
	}
}

static void openstack_metadata_not_implemented(GNode* node) {
	LOG(MOD "Metadata '%s' not implemented yet\n", (char*)node->parent->data);
}

static void openstack_metadata_keys(GNode* node) {
	GString* ssh_key;
	while (node) {
		if (g_strcmp0("data", node->data) == 0) {
			LOG(MOD "keys processing %s\n", (char*)node->data);
			ssh_key = g_string_new(node->children->data);
			if (!write_ssh_keys(ssh_key, DEFAULT_USER_USERNAME)) {
				LOG(MOD "Cannot Write ssh key\n");
			}
			g_string_free(ssh_key, true);
		} else {
			LOG(MOD "keys nothing to do with %s\n", (char*)node->data);
		}
		node = node->next;
	}
}

static void openstack_metadata_hostname(GNode* node) {
	gchar command[LINE_MAX];
	g_snprintf(command, LINE_MAX, HOSTNAMECTL_PATH " set-hostname '%s'", (char*)node->data);
	exec_task(command);
}

static void openstack_metadata_files(GNode* node) {
	gchar content_path[LINE_MAX] = { 0 };
	gchar path[LINE_MAX] = { 0 };
	gchar src_content_file[PATH_MAX] = { 0 };
	gchar* tmp_content_file = 0;
	while (node) {
		if (g_strcmp0("content_path", node->data) == 0) {
			if (node->children) {
				g_strlcpy(content_path, node->children->data, LINE_MAX);
			}
		} else if (g_strcmp0("path", node->data) == 0) {
			if (node->children) {
				g_strlcpy(path, node->children->data, LINE_MAX);
			}
		} else {
			content_path[0] = 0;
			path[0] = 0;
			LOG(MOD "files nothing to do with %s\n", (char*)node->data);
		}

		if (content_path[0] && path[0]) {
			switch (data_source) {
			case SOURCE_CONFIG_DRIVE:
				g_snprintf(src_content_file, PATH_MAX, "%s/openstack/%s",
				           config_drive_path, content_path);
				if (!copy_file(src_content_file, path)) {
					LOG(MOD "Copy file '%s' failed\n", src_content_file);
				}
			break;

			case SOURCE_METADATA_SERVICE:
				g_snprintf(src_content_file, PATH_MAX, "%s/%s", OPENSTACK_URL, content_path );
				tmp_content_file = curl_fetch_file(curl, src_content_file, 1, 0);
				if (!tmp_content_file) {
					LOG("Fetch file '%s' failed\n", src_content_file);
					break;
				}
				if (!copy_file(tmp_content_file, path)) {
					LOG(MOD "Copy file '%s' failed\n", tmp_content_file);
					break;
				}
				remove(tmp_content_file);
				g_free(tmp_content_file);
			break;

			case SOURCE_NONE:
				if (!copy_file(content_path, path)) {
					LOG(MOD "Copy file '%s' failed\n", content_path);
				}
			break;
			}
		}

		node = node->next;
	}
}
