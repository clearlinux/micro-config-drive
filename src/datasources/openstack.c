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

#include <glib.h>
#include <json-glib/json-glib.h>

#include "openstack.h"
#include "handlers.h"
#include "curl.h"
#include "lib.h"
#include "userdata.h"
#include "json.h"
#include "default_user.h"

#define MOD "openstack: "
#define USERDATA_URL "http://169.254.169.254/openstack/latest/user_data"
#define METADATA_URL "http://169.254.169.254/openstack/latest/meta_data.json"

int openstack_main(bool first_boot);

static int openstack_metadata(CURL* curl);
static int openstack_userdata(CURL* curl);

static void openstack_item(GNode* node, gpointer data);
static gboolean openstack_node_free(GNode* node, gpointer data);

static void openstack_metadata_not_implemented(GNode* node);
static void openstack_metadata_keys(GNode* node);
static void openstack_metadata_hostname(GNode* node);

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

int openstack_main(bool first_boot) {
	int result_code = EXIT_FAILURE;
	CURL* curl = NULL;

	if (!curl_common_init(&curl)) {
		LOG(MOD "Curl initialize failed\n");
		goto clean;
	}

	if (first_boot) {
		if (openstack_metadata(curl) != EXIT_SUCCESS) {
			LOG(MOD "Get and process metadata fail\n");
			goto clean;
		}
	}

	result_code = EXIT_SUCCESS;

	if (openstack_userdata(curl) != EXIT_SUCCESS) {
		LOG(MOD "No userdata provided to this machine\n");
		goto clean;
	}

clean:
	curl_easy_cleanup(curl);
	return result_code;
}

int openstack_process_metadata(const gchar* filename) {
	GError* error = NULL;
	JsonParser* parser = NULL;
	GNode* node = NULL;
	int result_code = EXIT_FAILURE;

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
	result_code = EXIT_SUCCESS;

fail:
	g_object_unref(parser);
	g_node_destroy(node);
	return result_code;
}

static int openstack_userdata(CURL* curl) {
	int result_code;
	gchar* data_filename = NULL;

	LOG(MOD "Fetching userdata file URL %s\n", USERDATA_URL );
	data_filename = curl_fetch_file(curl, USERDATA_URL, 1, 0);
	if (!data_filename) {
		LOG(MOD "Fetch userdata failed\n");
		return EXIT_FAILURE;
	}

	result_code = userdata_process_file(data_filename);
	g_free(data_filename);
	return result_code;
}

static int openstack_metadata(CURL* curl) {
	gchar* data_filename = NULL;
	int result_code;

	LOG(MOD "Fetching metadata file URL %s\n", METADATA_URL );
	data_filename = curl_fetch_file(curl, METADATA_URL, 10, 300000);
	if (!data_filename) {
		LOG(MOD "Fetch metadata failed\n");
		return EXIT_FAILURE;
	}

	result_code = openstack_process_metadata(data_filename);
	g_free(data_filename);
	return result_code;
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
	g_snprintf(command, LINE_MAX, "hostnamectl set-hostname '%s'", (char*)node->data);
	exec_task(command);
}
