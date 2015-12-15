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
#define USERDATA_URL "http://169.254.169.254/openstack/latest/user_data"
#define METADATA_URL "http://169.254.169.254/openstack/latest/meta_data.json"
#define USERDATA_DRIVE_PATH "/openstack/latest/user_data"
#define METADATA_DRIVE_PATH "/openstack/latest/meta_data.json"
#define ATTEMPTS 10
#define U_SLEEP 300000

int openstack_main(struct datasource_options_struct* opts);

static gboolean openstack_use_metadata_service(void);
static gboolean openstack_use_config_drive(void);

static gboolean openstack_metadata(CURL* curl);
static gboolean openstack_userdata(CURL* curl);

static void openstack_item(GNode* node, gpointer data);
static gboolean openstack_node_free(GNode* node, gpointer data);

static void openstack_metadata_not_implemented(GNode* node);
static void openstack_metadata_keys(GNode* node);
static void openstack_metadata_hostname(GNode* node);

static struct datasource_options_struct* options;

struct openstack_metadata_data {
	const gchar* key;
	void (*func)(GNode* node);
};

static struct openstack_metadata_data openstack_metadata_options[] = {
	{"random_seed",         openstack_metadata_not_implemented      },
	{"uuid",		openstack_metadata_not_implemented	},
	{"availability_zone",	openstack_metadata_not_implemented      },
	{"keys",	        openstack_metadata_keys                 },
	{"hostname",		openstack_metadata_hostname             },
	{"launch_index",	openstack_metadata_not_implemented      },
	{"public_keys",	        openstack_metadata_not_implemented      },
	{"project_id",	        openstack_metadata_not_implemented      },
	{"name",	        openstack_metadata_not_implemented      },
	{"files",	        openstack_metadata_not_implemented      },
	{"meta",	        openstack_metadata_not_implemented      },
	{NULL}
};

struct datasource_handler_struct openstack_datasource = {
	.datasource="openstack",
	.handler=&openstack_main
};

int openstack_main(struct datasource_options_struct* opts) {
	options = opts;

	if (openstack_use_config_drive()) {
		LOG(MOD "Metadata and userdata were processed using config drive\n");
		return EXIT_SUCCESS;
	} else if (openstack_use_metadata_service()) {
		LOG(MOD "Metadata and userdata were processed using metadata service\n");
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

gboolean openstack_process_metadata(const gchar* filename) {
	GError* error = NULL;
	JsonParser* parser = NULL;
	GNode* node = NULL;
	gboolean result = false;

	parser = json_parser_new();
	json_parser_load_from_file(parser, filename, &error);
	if (error) {
		LOG(MOD "Unable to parse '%s': %s\n", filename, error->message);
		g_error_free(error);
		goto fail;
	}

	node = g_node_new(g_strdup(filename));
	json_parse(json_parser_get_root(parser), node, false);
	cloud_config_dump(node);

	g_node_children_foreach(node, G_TRAVERSE_ALL, openstack_item, NULL);
	g_node_traverse(node, G_POST_ORDER, G_TRAVERSE_ALL, -1, openstack_node_free, NULL);
	result = true;

fail:
	g_object_unref(parser);
	g_node_destroy(node);
	return result;
}


static gboolean openstack_use_metadata_service(void) {
	gboolean result = false;
	CURL* curl = NULL;

	if (!curl_common_init(&curl)) {
		LOG(MOD "Curl initialize failed\n");
		goto cleancurl;
	}

	if (options->metadata) {
		if (!openstack_metadata(curl)) {
			LOG(MOD "Get and process metadata failed\n");
			goto cleancurl;
		}
	}

	result = true;

	if (options->user_data) {
		if (!openstack_userdata(curl)) {
			LOG(MOD "No userdata provided to this machine\n");
		}
	}
cleancurl:
	curl_easy_cleanup(curl);
	return result;
}

static gboolean openstack_use_config_drive(void) {
	char mountpoint[] = "/tmp/config-2-XXXXXX";
	gboolean config_drive = false;
	GString* metadata_drive_path;
	GString* userdata_drive_path;
	gchar* device;
	gchar* devtype;
	gboolean result = false;

	config_drive = disk_by_label("config-2", &device, &devtype);

	if (!config_drive) {
		LOG(MOD "Config drive not found\n");
		return false;
	}

	if (!mkdtemp(mountpoint)) {
		LOG(MOD "Cannot create directory '%s'\n", mountpoint);
		return false;
	}

	if (mount(device, mountpoint, devtype, MS_NODEV, NULL) != 0) {
		LOG(MOD "Cannot mount config drive\n");
		goto failcfgdrive1;
	}

	metadata_drive_path = g_string_new(mountpoint);
	g_string_append(metadata_drive_path, METADATA_DRIVE_PATH);
	userdata_drive_path = g_string_new(mountpoint);
	g_string_append(userdata_drive_path, USERDATA_DRIVE_PATH);

	if (!openstack_process_metadata(metadata_drive_path->str)) {
		LOG(MOD "Using config drive get and process metadata failed\n");
		goto failcfgdrive2;
	}
	result = true;

	if (!userdata_process_file(userdata_drive_path->str)) {
		LOG(MOD "Using config drive no userdata provided to this machine\n");
	}

failcfgdrive2:
	g_string_free(metadata_drive_path, true);
	g_string_free(userdata_drive_path, true);
	if (umount(mountpoint) != 0) {
		LOG(MOD "Using config drive cannot umount '%s'\n", mountpoint);
	}
failcfgdrive1:
	rmdir(mountpoint);
	return result;
}

static gboolean openstack_userdata(CURL* curl) {
	gboolean result;
	int attempts = ATTEMPTS;
	useconds_t u_sleep = U_SLEEP;
	gchar* data_filename = NULL;

	/*
	* if metadata was downloaded, then we do not need to wait
	* for nova metadata service because at this point it is
	* already up (running)
	*/
	if (options->metadata) {
		attempts = 1;
		u_sleep = 0;
	}

	LOG(MOD "Fetching userdata file URL %s\n", USERDATA_URL );
	data_filename = curl_fetch_file(curl, USERDATA_URL, attempts, u_sleep);
	if (!data_filename) {
		LOG(MOD "Fetch userdata failed\n");
		return false;
	}

	result = userdata_process_file(data_filename);
	g_free(data_filename);
	return result;
}

static gboolean openstack_metadata(CURL* curl) {
	gchar* data_filename = NULL;
	gboolean result;

	LOG(MOD "Fetching metadata file URL %s\n", METADATA_URL );
	data_filename = curl_fetch_file(curl, METADATA_URL, ATTEMPTS, U_SLEEP);
	if (!data_filename) {
		LOG(MOD "Fetch metadata failed\n");
		return false;
	}

	result = openstack_process_metadata(data_filename);
	g_free(data_filename);
	return result;
}

static gboolean openstack_node_free(GNode* node, __unused__ gpointer data) {
	if (node->data) {
		g_free(node->data);
	}

	return false;
}

static void openstack_item(GNode* node, __unused__ gpointer data) {
	size_t i;
	if (node->data) {
		for (i = 0; openstack_metadata_options[i].key != NULL; ++i) {
			if (g_strcmp0(node->data, openstack_metadata_options[i].key) == 0) {
				LOG(MOD "Metadata using %s handler\n", (char*)node->data);
				openstack_metadata_options[i].func(node->children);
				return;
			}
		}
		LOG(MOD "Metadata no handler for %s.\n", (char*)node->data);
	}
}

static void openstack_metadata_not_implemented(__unused__ GNode* node) {
	LOG(MOD "Not implemented yet\n");
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
