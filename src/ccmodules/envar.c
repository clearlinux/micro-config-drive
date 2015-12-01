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
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>

#include "handlers.h"
#include "lib.h"

#define MOD "envar: "

#define PROFILE_PATH SYSCONFDIR "/profile.d"

static GString* buffer_envar = NULL;

static void envar_item(GNode* node, gpointer data) {
	if (!node->data || !data) {
		g_node_children_foreach(node, G_TRAVERSE_ALL,
			envar_item, node->data ? node->data : data);
	} else {
		g_string_append_printf(buffer_envar, "export %s=\"%s\"\n",
			(char*)data, (char*)node->data );
		g_setenv(data, node->data, true);
	}
}

void envar_handler(GNode *node) {
	gchar profile_file[PATH_MAX] = { 0 };
	buffer_envar = g_string_new("");

	LOG(MOD "Groups Handler running...\n");
	g_node_children_foreach(node, G_TRAVERSE_ALL,
		envar_item, NULL);

	g_strlcat(profile_file, PROFILE_PATH, PATH_MAX);
	if (make_dir(profile_file, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
		LOG(MOD "Cannot create directory '%s'\n", (char*)profile_file);
		goto fail;
	}

	g_strlcat(profile_file, "/cloud-init.sh", PATH_MAX);

	if (!write_file(buffer_envar, profile_file, O_CREAT|O_APPEND|O_WRONLY,
			S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
		LOG(MOD "Cannot write environment variables\n");
		goto fail;
	}

fail:
	g_string_free(buffer_envar, true);
	buffer_envar = NULL;
}

struct cc_module_handler_struct envar_cc_module = {
	.name = "envar",
	.handler = &envar_handler
};
