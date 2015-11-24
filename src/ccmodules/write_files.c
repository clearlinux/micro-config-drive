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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include "handlers.h"
#include "cloud_config.h"
#include "lib.h"

#define MOD "write_files: "

static void write_files_item(GNode* node, __unused__ gpointer data) {
	const GNode* content;
	const GNode* path;
	const GNode* permissions;
	const GNode* owner;
	gchar **tokens;
	guint tokens_size;
	mode_t mode;
	const gchar* username = "";
	const gchar* groupname = "";

	CLOUD_CONFIG_KEY(CONTENT, "content");
	CLOUD_CONFIG_KEY(PATH, "path");
	CLOUD_CONFIG_KEY(OWNER, "owner");
	CLOUD_CONFIG_KEY(PERMISSIONS, "permissions");

	content = cloud_config_find(node, CONTENT);
	if (!content) {
		LOG(MOD "Unable to write file without \"content\" value.\n");
		return;
	}

	path = cloud_config_find(node, PATH);
	if (!path) {
		LOG(MOD "Unable to write file without \"path\" value.\n");
		return;
	}

	permissions = cloud_config_find(node, PERMISSIONS);
	owner = cloud_config_find(node, OWNER);

	/* assure the folder exists, and create if nexessary */
	char* dir = strdup((char *)path->data);
	dir = dirname(dir);
	int r = access(dir, W_OK);
	if (r == -1) {
		if (errno & ENOENT) {
			LOG(MOD "Creating part or all of %s\n", dir);
			gchar command[LINE_MAX];
			command[0] = 0;
			g_snprintf(command, LINE_MAX, "mkdir -p %s", dir);
			exec_task(command);
		} else {
			LOG(MOD "Path error: %s", strerror(errno));
			free(dir);
			return;
		}
	}
	free(dir);

	LOG(MOD "Writing to file %s: %s\n", (char*)path->data, (char*)content->data);

	const int fd = open(path->data, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		LOG(MOD "Cannot open %s.\n", (char*)path->data);
		return;
	}

	write(fd, content->data, strlen(content->data));

	if (permissions) {
		if (cloud_config_int_base(permissions, (int *)&mode, 8)) {
			fchmod(fd, mode);
		}
	}

	close(fd);

	if (owner) {
		tokens = g_strsplit_set(owner->data, ":.", 2);
		tokens_size = g_strv_length(tokens);
		if (tokens_size > 0) {
			username = tokens[0];
			if (tokens_size > 1) {
				groupname = tokens[1];
			}
			chown_path(path->data, username, groupname);
		}
		g_strfreev(tokens);
	}
}

void write_files_handler(GNode *node) {
	LOG(MOD "Write Files Handler running...\n");
	g_node_children_foreach(node, G_TRAVERSE_ALL, write_files_item, NULL);
}

struct cc_module_handler_struct write_files_cc_module = {
	.name = "write_files",
	.handler = &write_files_handler
};

